#include "Engine/Net/NetMessage.hpp"
#include "Engine/Net/ProtoReflect.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Engine/Net/RecipientFilter.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Events/EventTypes.hpp>
#include <VoltMod/Events/GameEvents.hpp>
#include <VoltMod/Hooks/Vote.hpp>
#include <engine/igameeventsystem.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include <string_view>
#include <utility>

namespace VoltMod
{

static constexpr std::string_view ControllerClass = "vote_controller";

// A client takes F1/F2 only while an issue is active. The engine must never run this one, so
// `vote` and `callvote` are blocked during a vote.
static constexpr int YesNoIssueIndex = 2;
static constexpr int NoIssue = -1;
static constexpr int AllTeams = -1;

// The vote_cast event's option numbers.
static constexpr int YesOption = 0;
static constexpr int NoOption = 1;

// Names carry the message prefix: a bare "VoteFailed" also matches CCSUsrMsg_CallVoteFailed.
static constexpr std::string_view VoteStartMessage = "CCSUsrMsg_VoteStart";
static constexpr std::string_view VotePassMessage = "CCSUsrMsg_VotePass";
static constexpr std::string_view VoteFailedMessage = "CCSUsrMsg_VoteFailed";

static ProtoMessage* AsProto(CNetMessage* message)
{
    return message ? message->ToPB<ProtoMessage>() : nullptr;
}

/** @ref ProtoField plus what a missing field costs here; see Engine/ProtoReflect.hpp for why the
 *  fields are reached by name at all. */
static const ProtoFieldDescriptor* VoteField(ProtoMessage* message, std::string_view name)
{
    const auto* field = ProtoField(*message, name);
    if (!field)
    {
        const auto* descriptor = message->GetDescriptor();
        Log::Warn("Vote: {} has no field '{}'; the panel may render incomplete.",
                  descriptor ? descriptor->name() : "<unknown>", name);
    }
    return field;
}

static void SetInt(ProtoMessage* message, std::string_view name, int32_t value)
{
    if (const auto* field = VoteField(message, name))
    {
        message->GetReflection()->SetInt32(message, field, value);
    }
}

static void SetBool(ProtoMessage* message, std::string_view name, bool value)
{
    if (const auto* field = VoteField(message, name))
    {
        message->GetReflection()->SetBool(message, field, value);
    }
}

static void SetString(ProtoMessage* message, std::string_view name, const std::string& value)
{
    if (const auto* field = VoteField(message, name))
    {
        message->GetReflection()->SetString(message, field, value);
    }
}

Vote::Vote(Interfaces& interfaces, EntitySystem& entities, PlayerManager& players, GameEvents& events,
           Scheduler& scheduler)
    : _interfaces(interfaces), _entities(entities), _players(players), _events(events), _scheduler(scheduler)
{
    _leaving = players.Disconnected += [this](Player& player) { DropVoter(player.Slot()); };
}

MultiRecipientFilter Vote::Recipients() const
{
    MultiRecipientFilter filter;
    for (const Player* player : _players.All())
    {
        if (!player->IsBot())
        {
            filter.AddRecipient(player->Slot());
        }
    }
    return filter;
}

Schema::CVoteController Vote::Controller()
{
    return Schema::CVoteController{_entities.Find(ControllerClass).Raw()};
}

bool Vote::Start(VoteRequest request)
{
    if (_running || !request.Passed)
    {
        return false;
    }

    Ballot ballot{.Request = std::move(request)};
    for (const Player* player : _players.All())
    {
        if (!player->IsBot() && IsValidSlot(player->Slot()))
        {
            ballot.Waiting[player->Slot()] = true;
            ++ballot.Tally.Eligible;
        }
    }
    if (ballot.Tally.Eligible == 0)
    {
        return false;
    }

    if (Schema::CVoteController controller = Controller())
    {
        controller.SetPotentialVotes(ballot.Tally.Eligible);
        controller.SetIsYesNoVote(true);
        // The VoteStart recipients decide who may vote.
        controller.SetOnlyTeamToVote(AllTeams);
        controller.SetActiveIssueIndex(YesNoIssueIndex);
    }

    Ballot& running = _running.emplace(std::move(ballot));
    PublishCounts(running.Tally);
    SendStart(running);
    running.Timeout = _scheduler.Delay(running.Request.DurationMs, [this] { Finish(VoteEndReason::TimeUp); });
    return true;
}

bool Vote::TryCastBallot(int slot, std::string_view option)
{
    if (!_running)
    {
        return false;
    }

    Ballot& ballot = *_running;
    if (!IsValidSlot(slot) || !ballot.Waiting[slot])
    {
        return true;
    }

    if (option == "option1")
    {
        ++ballot.Tally.Yes;
        PublishBallot(slot, YesOption);
    }
    else if (option == "option2")
    {
        ++ballot.Tally.No;
        PublishBallot(slot, NoOption);
    }
    else
    {
        return true;
    }

    ballot.Waiting[slot] = false;
    PublishCounts(ballot.Tally);
    CloseIfAllVoted();
    return true;
}

void Vote::DropVoter(int slot)
{
    if (!_running || !IsValidSlot(slot) || !_running->Waiting[slot])
    {
        return;
    }

    // A player who leaves without voting would otherwise hold the vote open until it times out.
    _running->Waiting[slot] = false;
    --_running->Tally.Eligible;
    PublishCounts(_running->Tally);
    CloseIfAllVoted();
}

void Vote::CloseIfAllVoted()
{
    if (_running->Tally.Cast() < _running->Tally.Eligible)
    {
        return;
    }

    // Deferred a tick: the engine may still be inside the command dispatch.
    _running->Close = _scheduler.NextTick([this] { Finish(VoteEndReason::AllVoted); });
}

void Vote::End(VoteEndReason reason)
{
    if (_running)
    {
        Finish(reason);
    }
}

void Vote::Finish(VoteEndReason reason)
{
    // Taken out first: the callbacks may start the next vote. Dropping it cancels both timers.
    Ballot ballot = std::move(*_running);
    _running.reset();

    // A cancelled vote never asks the caller whether it passed.
    const bool passed = reason != VoteEndReason::Cancelled && ballot.Request.Passed(ballot.Tally);

    SendOutcome(ballot.Request, passed);

    if (Schema::CVoteController controller = Controller())
    {
        controller.SetActiveIssueIndex(NoIssue);
    }

    if (ballot.Request.Finished)
    {
        ballot.Request.Finished(passed, reason);
    }
}

void Vote::PublishBallot(int slot, int option)
{
    // The voter's panel registers the key press from this event.
    IGameEvent* event = _events.CreateEvent(VoteCast::EventName);
    if (!event)
    {
        return;
    }

    event->SetInt("vote_option", option);
    event->SetInt("team", AllTeams);
    event->SetPlayer("userid", CPlayerSlot(slot));
    _events.FireEvent(event);
}

void Vote::PublishCounts(const VoteTally& tally)
{
    // The panel reads its tally from this event.
    IGameEvent* event = _events.CreateEvent(VoteChanged::EventName);
    if (!event)
    {
        return;
    }

    event->SetInt("vote_option1", tally.Yes);
    event->SetInt("vote_option2", tally.No);
    event->SetInt("vote_option3", 0);
    event->SetInt("vote_option4", 0);
    event->SetInt("vote_option5", 0);
    event->SetInt("potentialVotes", tally.Eligible);
    _events.FireEvent(event);
}

void Vote::SendStart(const Ballot& ballot)
{
    MultiRecipientFilter filter = Recipients();
    PostUserMessage(_interfaces, _voteStartInternal, VoteStartMessage, filter, [&ballot](CNetMessage* raw) {
        auto* start = AsProto(raw);
        if (!start)
        {
            return false;
        }
        SetInt(start, "team", AllTeams);
        SetInt(start, "player_slot", ballot.Request.Caller);
        SetInt(start, "vote_type", -1);
        SetString(start, "disp_str", ballot.Request.Title);
        SetString(start, "details_str", ballot.Request.Detail);
        SetBool(start, "is_yes_no_vote", true);
        return true;
    });
}

void Vote::SendOutcome(const VoteRequest& request, bool passed)
{
    // Pass and fail are distinct message types, so each gets its own cache slot.
    auto& cached = passed ? _votePassInternal : _voteFailedInternal;
    MultiRecipientFilter filter = Recipients();

    PostUserMessage(_interfaces, cached, passed ? VotePassMessage : VoteFailedMessage, filter,
                    [&request, passed](CNetMessage* raw) {
                        auto* outcome = AsProto(raw);
                        if (!outcome)
                        {
                            return false;
                        }
                        SetInt(outcome, "team", AllTeams);
                        if (passed)
                        {
                            SetInt(outcome, "vote_type", -1);
                            SetString(outcome, "disp_str", request.Title);
                            SetString(outcome, "details_str", request.Detail);
                        }
                        else
                        {
                            SetInt(outcome, "reason", 0);
                        }
                        return true;
                    });
}

}  // namespace VoltMod
