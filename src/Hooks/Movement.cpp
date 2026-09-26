#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Hooks/Movement.hpp>
#include <VoltMod/Schema/Api.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <algorithm>
#include <cs_usercmd.pb.h>

namespace VoltMod
{

Movement::Movement(EntitySystem& entities, const Bindings& bindings)
    : _hook("Movement", [this] { return Install(); }),
      Rewrite(_hook.ForEvent()),
      Before(_hook.ForEvent()),
      After(_hook.ForEvent()),
      _entities(entities),
      _bindings(bindings)
{}

Result<Subscription> Movement::Install()
{
    if (!_bindings.UserCmdProto)
    {
        Log::Warn("Movement: no usable 'CUserCmd::CSGOUserCmdPB' offset; handlers get Valid=false commands.");
    }

    if (!_bindings.UserCmdNumber)
    {
        Log::Warn(
            "Movement: no usable 'CUserCmdBase::cmdNum' offset; falling back to the protobuf's "
            "legacy_command_number, which the live client leaves at 0.");
    }

    return HookVirtual(
        "Movement RunCommand", _bindings.RunCommand,
        [this](EngineMovementServices& services, void* userCmd) {
            _slot = OwnerSlot(&services);
            if (!IsValidSlot(_slot))
            {
                return;
            }
            Decode(userCmd);
            Rewrite.Raise(_slot, _cmd);
            Before.Raise(_slot, _cmd);
        },
        [this](EngineMovementServices&, void* /*userCmd*/) {
            if (IsValidSlot(_slot))
            {
                After.Raise(_slot, _cmd);
            }
        });
}

Status Movement::Available() const
{
    if (!_bindings.RunCommand)
    {
        return std::unexpected(Error::Unsupported("the CPlayer_MovementServices::RunCommand vtable slot did not bind"));
    }
    if (!_bindings.UserCmdProto)
    {
        return std::unexpected(Error::Unsupported("the CUserCmd::CSGOUserCmdPB offset did not bind"));
    }
    return {};
}

int Movement::OwnerSlot(void* movementServices)
{
    // Resolve the controller from the pawn instead of scanning the roster.
    CEntityInstance* pawn = Schema::CPlayer_MovementServices{movementServices}.OwnerEntity();
    return pawn ? Pawn{_entities, pawn}.Slot() : -1;
}

void Movement::Decode(const void* userCmd)
{
    _cmd = {};
    if (!userCmd || !_bindings.UserCmdProto)
    {
        return;
    }

    const auto* pb = static_cast<const CSGOUserCmdPB*>(_bindings.UserCmdProto.Ptr(userCmd));
    const auto& base = pb->base();

    _cmd.Valid = true;
    _cmd.ClientTick = base.client_tick();
    // Live clients store the command number in the wrapper, not the protobuf payload.
    _cmd.CommandNumber = _bindings.UserCmdNumber ? _bindings.UserCmdNumber.Read(userCmd) : base.legacy_command_number();
    _cmd.HasViewAngles = base.has_viewangles();
    if (_cmd.HasViewAngles)
    {
        _cmd.ViewPitch = base.viewangles().x();
        _cmd.ViewYaw = base.viewangles().y();
        _cmd.ViewRoll = base.viewangles().z();
    }
    _cmd.ForwardMove = base.forwardmove();
    _cmd.LeftMove = base.leftmove();
    if (base.has_buttons_pb())
    {
        _cmd.ButtonsHeld = base.buttons_pb().buttonstate1();
        _cmd.ButtonsChanged = base.buttons_pb().buttonstate2();
    }
    _cmd.MouseDx = base.mousedx();
    _cmd.MouseDy = base.mousedy();
    _cmd.Attack1StartHistoryIndex = pb->attack1_start_history_index();
    _cmd.Attack2StartHistoryIndex = pb->attack2_start_history_index();

    _cmd.SubtickMoveCount = std::min(base.subtick_moves_size(), PlayerInput::MaxSubtickMoves);
    for (int i = 0; i < _cmd.SubtickMoveCount; ++i)
    {
        const auto& move = base.subtick_moves(i);
        _cmd.SubtickMoves[i] = {
            .Button = move.button(),
            .Pressed = move.pressed(),
            .When = move.when(),
            .PitchDelta = move.pitch_delta(),
            .YawDelta = move.yaw_delta(),
        };
    }

    _cmd.InputHistorySent = pb->input_history_size();
    _cmd.InputHistoryCount = std::min(_cmd.InputHistorySent, PlayerInput::MaxInputHistory);
    for (int i = 0; i < _cmd.InputHistoryCount; ++i)
    {
        const auto& entry = pb->input_history(i);
        auto& sample = _cmd.InputHistorySamples[i];
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
