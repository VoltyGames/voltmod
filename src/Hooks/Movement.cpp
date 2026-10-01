#include <VoltMod/Hooks/Movement.hpp>
#include <utility>

namespace VoltMod
{

Movement::Movement(const Bindings& bindings, Connector connect)
    : _hook("Movement",
            [this]() -> Result<Subscription> {
                if (Status available = Available(); !available)
                {
                    return std::unexpected(available.error());
                }
                return _connect(*this);
            }),
      Rewrite(_hook.ForEvent()),
      Before(_hook.ForEvent()),
      After(_hook.ForEvent()),
      _bindings(bindings),
      _connect(std::move(connect))
{}

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

void Movement::BeforeCommand(int slot, const PlayerInput& input)
{
    // Every plugin sees the host's decode; only a rewrite needs a copy of its own.
    _isRewritten = !Rewrite.Empty();
    if (!_isRewritten)
    {
        Before.Raise(slot, input);
        return;
    }
    _rewritten = input;
    Rewrite.Raise(slot, _rewritten);
    Before.Raise(slot, _rewritten);
}

void Movement::AfterCommand(int slot, const PlayerInput& input)
{
    After.Raise(slot, _isRewritten ? _rewritten : input);
}

}  // namespace VoltMod
