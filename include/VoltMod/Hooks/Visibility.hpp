#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Engine/Color.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Team.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace VoltMod
{

/** Glow colors and an optional per-slot veto. */
struct GlowConfig
{
    Color TerroristColor{255, 128, 0};
    Color CtColor{0, 160, 255};
    /** Extra veto on top of the live, team and hidden checks; empty glows everyone. */
    std::function<bool(int slot)> Filter;
};

/**
 * @brief Which clients receive which entities, filtered after CheckTransmit.
 *
 * A hidden pawn takes its weapons, wearables and shadow with it but stays visible to whoever
 * spectates it; a hidden controller loses its scoreboard row and chat attribution. Sounds are
 * networked separately and are unaffected.
 */
class Visibility
{
public:
    /** @p slots resets a slot's hiding when it changes hands. All three must outlive this. */
    Visibility(EntitySystem& entities, const Bindings& bindings, SlotEvents& slots);
    Visibility(const Visibility&) = delete;
    Visibility& operator=(const Visibility&) = delete;

    /** Hide or show `slot`'s pawn from every other client. */
    void SetPawnHidden(int slot, bool hidden);

    /** Hide or show `slot`'s controller from every other client. */
    void SetControllerHidden(int slot, bool hidden);

    bool IsPawnHidden(int slot) const;

    /** Network `entity` to `slot` alone. Calling again moves it to another slot. */
    void ShowOnlyTo(EntityRef entity, int slot);

    /** Network `entity` to `team` alone, spectators excluded. Calling again moves it. */
    void ShowOnlyToTeam(EntityRef entity, Team team);

    /** Keep `entity` from every client on `team`. Calling again moves it. */
    void HideFromTeam(EntityRef entity, Team team);

    /** Undo the calls above; a removed entity needs no call. */
    void ShowToEveryone(EntityRef entity);

    /** A @ref GlowVision for @p viewerSlot alone; its clones go with the last owner. */
    std::shared_ptr<GlowVision> CreateGlow(int viewerSlot, GlowConfig config = {});

    /** A @ref GlowVision the whole of @p viewerTeam shares. */
    std::shared_ptr<GlowVision> CreateGlow(Team viewerTeam, GlowConfig config = {});

    /** @internal Called by the framework after ISource2GameEntities::CheckTransmit. */
    void OnCheckTransmit(CCheckTransmitInfo** infoList, int infoCount);

    /** An error when the CheckTransmitPlayerSlot offset did not bind; every call is then inert. */
    Status Available() const;

private:
    struct SlotState
    {
        bool PawnHidden = false;
        bool ControllerHidden = false;

        bool Any() const { return PawnHidden || ControllerHidden; }
    };

    /** Sent to @ref Viewer alone, else to @ref ShownTo alone, else to everyone off @ref HiddenFrom. */
    struct PrivateEntity
    {
        EntityRef Entity;
        int Viewer = -1;
        Team ShownTo = Team::None;
        Team HiddenFrom = Team::None;
        int Index = -1;  ///< resolved once per snapshot

        bool HiddenFor(int recipient, Team recipientTeam) const;
    };

    /** Replaces the entry for the same entity, or adds it. */
    void SetPrivate(const PrivateEntity& entry);

    EntitySystem& _entities;
    const Bindings& _bindings;
    std::array<SlotState, MaxPlayers> _state{};
    std::vector<PrivateEntity> _private;
    /** Declared last: unregisters before the state it resets. */
    Subscription _slotListener;
};

}  // namespace VoltMod
