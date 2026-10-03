#include "Host/Input/PlayerCommands.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Schema/Api.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <algorithm>
#include <cs_usercmd.pb.h>
#include <entityhandle.h>

namespace VoltMod
{

/** The slot of the player whose pawn owns @p movementServices, or -1. */
static int OwnerSlot(void* movementServices)
{
    CEntityInstance* pawn = Schema::CPlayer_MovementServices{movementServices}.OwnerEntity();
    if (!pawn)
    {
        return -1;
    }
    // Controllers sit at entity index slot + 1.
    const EntityRef controller = Schema::CBasePlayerPawn{pawn}.ControllerRef();
    const int slot = CEntityHandle(controller.Handle).GetEntryIndex() - 1;
    return IsValidSlot(slot) ? slot : -1;
}

PlayerCommands::PlayerCommands(const Bindings& bindings, PluginRegistry& registry)
    : _registry(registry), _bindings(bindings)
{
    if (Status installed = Install(); !installed)
    {
        Log::Warn("Player commands: {}; Movement is off.", installed.error().Detail);
        return;
    }
    if (!_bindings.UserCmdNumber)
    {
        Log::Warn("Player commands: no 'CUserCmdBase::cmdNum' offset; command numbers read 0.");
    }
}

Status PlayerCommands::Install()
{
    if (!_bindings.RunCommand || !_bindings.UserCmdProto)
    {
        return std::unexpected(Error::Unsupported("the RunCommand slot or the usercmd offset did not bind"));
    }

    const auto beforeCommand = [this](EngineMovementServices& services, void* userCmd) { Before(&services, userCmd); };
    const auto afterCommand = [this](EngineMovementServices&, void*) { After(); };
    auto hook = HookVirtual("Player commands", _bindings.RunCommand, beforeCommand, afterCommand);
    if (!hook)
    {
        return std::unexpected(hook.error());
    }
    _hook = std::move(*hook);
    return {};
}

void PlayerCommands::Before(void* movementServices, const void* userCmd)
{
    _slot = _registry.HasCommandListeners() ? OwnerSlot(movementServices) : -1;
    if (_slot < 0)
    {
        return;
    }
    Decode(userCmd);
    _registry.RaisePlayerCommand(_slot, _input);
}

void PlayerCommands::After()
{
    if (_slot >= 0)
    {
        _registry.RaisePlayerCommandDone(_slot, _input);
    }
}

void PlayerCommands::Decode(const void* userCmd)
{
    _input = {};
    if (!userCmd)
    {
        return;
    }

    const auto* pb = static_cast<const CSGOUserCmdPB*>(_bindings.UserCmdProto.Ptr(userCmd));
    const auto& base = pb->base();

    _input.Valid = true;
    _input.ClientTick = base.client_tick();
    // Live clients store the command number in the wrapper, not the protobuf payload.
    _input.CommandNumber =
        _bindings.UserCmdNumber ? _bindings.UserCmdNumber.Read(userCmd) : base.legacy_command_number();
    _input.HasViewAngles = base.has_viewangles();
    if (_input.HasViewAngles)
    {
        _input.ViewPitch = base.viewangles().x();
        _input.ViewYaw = base.viewangles().y();
        _input.ViewRoll = base.viewangles().z();
    }
    _input.ForwardMove = base.forwardmove();
    _input.LeftMove = base.leftmove();
    if (base.has_buttons_pb())
    {
        _input.ButtonsHeld = base.buttons_pb().buttonstate1();
        _input.ButtonsChanged = base.buttons_pb().buttonstate2();
    }
    _input.MouseDx = base.mousedx();
    _input.MouseDy = base.mousedy();
    _input.Attack1StartHistoryIndex = pb->attack1_start_history_index();
    _input.Attack2StartHistoryIndex = pb->attack2_start_history_index();

    _input.SubtickMoveCount = std::min(base.subtick_moves_size(), PlayerInput::MaxSubtickMoves);
    for (int i = 0; i < _input.SubtickMoveCount; ++i)
    {
        const auto& move = base.subtick_moves(i);
        _input.SubtickMoves[i] = {
            .Button = move.button(),
            .Pressed = move.pressed(),
            .When = move.when(),
            .PitchDelta = move.pitch_delta(),
            .YawDelta = move.yaw_delta(),
        };
    }

    _input.InputHistorySent = pb->input_history_size();
    _input.InputHistoryCount = std::min(_input.InputHistorySent, PlayerInput::MaxInputHistory);
    for (int i = 0; i < _input.InputHistoryCount; ++i)
    {
        const auto& entry = pb->input_history(i);
        auto& sample = _input.InputHistorySamples[i];
        sample.TargetEntIndex = entry.target_ent_index();
        if (entry.has_view_angles())
        {
            sample.HasViewAngles = true;
            sample.ViewPitch = entry.view_angles().x();
            sample.ViewYaw = entry.view_angles().y();
        }
    }
}

}  // namespace VoltMod
