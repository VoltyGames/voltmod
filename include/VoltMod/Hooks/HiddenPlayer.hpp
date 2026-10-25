#pragma once

#include <VoltMod/Engine/Team.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/GlowVision.hpp>
#include <VoltMod/Hooks/Visibility.hpp>
#include <string>

namespace VoltMod
{

/**
 * @brief A player turned into a nameless spectator nobody else sees, who sees every player through walls.
 *
 * @ref Show, or destroying this, gives back the name and the team; a team picked meanwhile wins.
 * Call @ref UpdateGlow every @ref GlowVision::RefreshIntervalMs.
 */
class HiddenPlayer
{
public:
    /** Hides @p slot now. Both services must outlive this. */
    HiddenPlayer(EntitySystem& entities, Visibility& visibility, int slot);
    ~HiddenPlayer() { Show(); }
    HiddenPlayer(const HiddenPlayer&) = delete;
    HiddenPlayer& operator=(const HiddenPlayer&) = delete;

    /** Rebuilds the glow across rounds, spawns and deaths. */
    void UpdateGlow();

    /** Shows the player again; later calls do nothing. */
    void Show();

private:
    EntitySystem& _entities;
    Visibility& _visibility;
    int _slot;
    Team _team = Team::None;
    std::string _name;
    GlowVision _glow;
    bool _hidden = true;
};

}  // namespace VoltMod
