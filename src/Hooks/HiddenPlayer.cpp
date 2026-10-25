#include <VoltMod/Entities/Controller.hpp>
#include <VoltMod/Hooks/HiddenPlayer.hpp>

namespace VoltMod
{

HiddenPlayer::HiddenPlayer(EntitySystem& entities, Visibility& visibility, int slot)
    : _entities(entities), _visibility(visibility), _slot(slot), _glow(entities, visibility, slot)
{
    // The controller keeps a team while dead or spectating; the pawn may not exist.
    const Controller controller = _entities.Controller(slot);
    _team = controller.TeamNum();
    _name = std::string(controller.PlayerName());
    // A blank name still hides the player if a CS2 update leaves the visibility filter inert.
    controller.SetPlayerName("");
    controller.ChangeTeam(Team::Spectator);
    _visibility.SetControllerHidden(slot, true);
    _glow.Refresh();
}

void HiddenPlayer::UpdateGlow()
{
    if (_hidden)
    {
        _glow.Refresh();
    }
}

void HiddenPlayer::Show()
{
    if (!_hidden)
    {
        return;
    }
    _hidden = false;
    _glow.Destroy();
    _visibility.SetControllerHidden(_slot, false);

    const Controller controller = _entities.Controller(_slot);
    if (!controller)
    {
        return;
    }
    controller.SetPlayerName(_name);
    if (IsPlaying(_team) && controller.TeamNum() == Team::Spectator)
    {
        controller.ChangeTeam(_team);
    }
}

}  // namespace VoltMod
