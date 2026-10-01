#pragma once

#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/Visibility.hpp>
#include <array>
#include <string>
#include <utility>

namespace VoltMod
{

/**
 * @brief Players as team-colored outlines through walls, for one client or one team alone.
 *
 * Each glowing player gets two clones following their pawn: an invisible relay and the glow prop on
 * it, which renders only the outline. Call @ref Refresh every @ref RefreshIntervalMs.
 */
class GlowVision
{
public:
    /** Suggested tick interval for @ref Refresh. */
    static constexpr int RefreshIntervalMs = 500;

    /** Both services must outlive this; `runtime.Visibility.CreateGlow` passes them. */
    GlowVision(EntitySystem& entities, Visibility& visibility, int viewerSlot, GlowConfig config = {})
        : _entities(entities), _visibility(visibility), _viewerSlot(viewerSlot), _config(std::move(config))
    {}
    GlowVision(EntitySystem& entities, Visibility& visibility, Team viewerTeam, GlowConfig config = {})
        : _entities(entities), _visibility(visibility), _viewerTeam(viewerTeam), _config(std::move(config))
    {}
    ~GlowVision() { Destroy(); }
    GlowVision(const GlowVision&) = delete;
    GlowVision& operator=(const GlowVision&) = delete;

    /** Match the clones to the live players. */
    void Refresh();

    /** Remove every clone now; @ref Refresh rebuilds them. */
    void Destroy();

private:
    struct GlowPair
    {
        EntityRef Relay;
        EntityRef Glow;
        VoltMod::Team Team = VoltMod::Team::None;
        std::string Model;

        bool Active() const { return static_cast<bool>(Relay); }
    };

    void CreatePair(int slot, GlowPair& pair);
    void DestroyPair(GlowPair& pair);
    void ShowToViewers(EntityRef entity);

    EntitySystem& _entities;
    Visibility& _visibility;
    int _viewerSlot = -1;
    Team _viewerTeam = Team::None;  ///< when there is no viewer slot
    GlowConfig _config;
    std::array<GlowPair, MaxPlayers> _pairs{};
};

}  // namespace VoltMod
