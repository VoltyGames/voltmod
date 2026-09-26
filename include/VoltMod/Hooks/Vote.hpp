#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Engine/Net/RecipientFilter.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Events/GameEvents.hpp>
#include <VoltMod/Players/PlayerManager.hpp>
#include <VoltMod/Schema/Generated/CVoteController.hpp>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace VoltMod
{

/** Why a vote stopped taking ballots. */
enum class VoteEndReason
{
    AllVoted,  ///< every eligible player cast a ballot
    TimeUp,
    Cancelled,  ///< an admin (or a peer feature) called it off
};

/** The tally handed to @ref VoteRequest::Passed. */
struct VoteTally
{
    int Eligible = 0;  ///< players who could vote
    int Yes = 0;
    int No = 0;

    int Cast() const { return Yes + No; }
};

/** A yes/no vote for @ref Vote::Start. */
struct VoteRequest
{
    /** A `#SFUI_vote...` or `#Panorama_vote...` token the client already has; other text does not render. */
    std::string Title;
    /** The token's detail string, often a map or player name. */
    std::string Detail;
    /** How long before the vote closes itself. */
    int DurationMs = 30000;
    /** Whose name the panel credits; -1 for the server. */
    int Caller = -1;
    /** Decides whether the vote passed. Called once when voting ends, never for a cancelled vote. */
    std::function<bool(const VoteTally&)> Passed;
    /** Called after the pass/fail panel is sent, with what @ref Passed decided. */
    std::function<void(bool passed, VoteEndReason reason)> Finished;
};

/**
 * @brief The game's yes/no vote panel (F1/F2), with the ballots counted here.
 *
 * One vote at a time, open to every connected human; bots get no panel and no ballot. Callbacks
 * run on the game thread.
 *
 * @code
 * runtime.Vote.Start({.Title = "#SFUI_vote_changelevel", .Detail = map, .DurationMs = 20000,
 *                     .Passed = [](const VoltMod::VoteTally& tally) { return tally.Yes > tally.No; }});
 * @endcode
 */
class Vote
{
public:
    /** All five must outlive this service; the Runtime declares them above it. */
    Vote(Interfaces& interfaces, EntitySystem& entities, PlayerManager& players, GameEvents& events,
         Scheduler& scheduler);
    Vote(const Vote&) = delete;
    Vote& operator=(const Vote&) = delete;

    /** Open @p request's vote. False when one is already running, it has no @ref VoteRequest::Passed,
     *  or no human is connected. */
    bool Start(VoteRequest request);

    /** End the running vote early. No-op when none is running. */
    void End(VoteEndReason reason);

    bool InProgress() const { return _running.has_value(); }

    /** @internal Called by the framework for every `vote <option>` console command, as the
     *  panel's F1/F2 keys send it. True while a vote is running: the engine must not see it. */
    bool TryCastBallot(int slot, std::string_view option);

private:
    struct Ballot
    {
        VoteRequest Request;
        VoteTally Tally;
        /** Eligible players who have not voted yet. */
        std::array<bool, MaxPlayers> Waiting{};
        /** The timeout, and the deferred close once every ballot is in. Owned here, so ending the
         *  vote cancels both. */
        Subscription Timeout;
        Subscription Close;
    };

    /** The map's vote controller, or an empty handle. Never stored: a map change frees it. */
    Schema::CVoteController Controller();
    void Finish(VoteEndReason reason);
    /** Stop waiting on @p slot, whose player left before voting. */
    void DropVoter(int slot);
    void CloseIfAllVoted();
    void SendStart(const Ballot& ballot);
    void SendOutcome(const VoteRequest& request, bool passed);
    void PublishBallot(int slot, int option);
    void PublishCounts(const VoteTally& tally);
    /** Every connected human - who a vote panel is sent to. */
    MultiRecipientFilter Recipients() const;

    Interfaces& _interfaces;
    EntitySystem& _entities;
    PlayerManager& _players;
    GameEvents& _events;
    Scheduler& _scheduler;

    /** Resolved message types, cached on first send; they are stable for the process. */
    INetworkMessageInternal* _voteStartInternal = nullptr;
    INetworkMessageInternal* _votePassInternal = nullptr;
    INetworkMessageInternal* _voteFailedInternal = nullptr;
    std::optional<Ballot> _running;
    /** Declared last so it unregisters before the ballot its handler edits. */
    Subscription _leaving;
};

}  // namespace VoltMod
