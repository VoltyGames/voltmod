#include "Ui/ScreenEntity.hpp"

#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Schema/Api.hpp>
#include <format>
#include <tier0/utlstring.h>
#include <tier0/utlvector.h>
#include <utility>

namespace VoltMod
{

using StringTable = CUtlVector<CUtlString>;

/** EHudPanelClassStatus_t values. */
static constexpr int32_t ClassAbsent = 0;
static constexpr int32_t ClassPresent = 1;

/** CS2 refuses to intern past this many entries per table; stop short so a runaway caller cannot fill them. */
static constexpr int StringTableCap = 1024;
static constexpr int StringTableHeadroom = 64;

/** Assigned in place: CUtlString allocates inside tier0, the allocator the engine frees with. */
static void SetStr(CUtlString& out, std::string_view text)
{
    out.SetDirect(text.data(), static_cast<int>(text.size()));
}

int ScreenEntity::PlayerStateCount() const
{
    // The embedded vector keeps its count first, which is what the engine itself reads before indexing.
    const Schema::CCSCustomHudLayout layout{_entities.Get(_entity).Raw()};
    return layout ? layout.PlayerLayoutStates() : -1;
}

Result<CEntityInstance*> ScreenEntity::EntityForWrite(int engineSlot) const
{
    Entity entity = _entities.Get(_entity);
    if (!entity)
    {
        return std::unexpected(Error::NotFound("the custom_hud_layout entity no longer exists"));
    }

    const Schema::CCSCustomHudLayout layout{entity.Raw()};
    for (const auto& [name, table] :
         {std::pair{"element id", layout.PanelIds()}, std::pair{"class name", layout.ClassNames()},
          std::pair{"text variable", layout.DialogVariableNames()}})
    {
        const int count = table ? static_cast<const StringTable*>(table)->Count() : -1;
        if (count >= StringTableCap - StringTableHeadroom)
        {
            return std::unexpected(
                Error::Failed(std::format("the {} table is nearly full ({}/{})", name, count, StringTableCap)));
        }
    }

    if (engineSlot == EveryoneSlot)
    {
        return entity.Raw();
    }

    // The engine's per-player setters return silently for a slot past the state count.
    if (const int states = PlayerStateCount(); states <= engineSlot)
    {
        return std::unexpected(Error::Failed(
            std::format("slot {} has no per-player layout state (the entity holds {})", engineSlot, states)));
    }

    return entity.Raw();
}

Status ScreenEntity::SendText(int engineSlot, std::string_view variable, std::string_view value)
{
    auto entity = EntityForWrite(engineSlot);
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    CUtlString root, name, text;
    SetStr(root, _layout.Name());
    SetStr(name, variable);
    SetStr(text, value);

    const Bindings& bindings = _entities.Bindings();
    if (engineSlot == EveryoneSlot)
    {
        if (!bindings.CustomHudSetDialogVariable)
        {
            return std::unexpected(Error::Unsupported("the custom HUD text setter did not bind"));
        }
        bindings.CustomHudSetDialogVariable(*entity, &root, &name, &text);
        return {};
    }

    if (!bindings.CustomHudSetDialogVariableForPlayer)
    {
        return std::unexpected(Error::Unsupported("the custom HUD per-player text setter did not bind"));
    }
    bindings.CustomHudSetDialogVariableForPlayer(*entity, engineSlot, &root, &name, &text);
    return {};
}

Status ScreenEntity::SendClass(int engineSlot, std::string_view elementId, std::string_view className, bool on)
{
    auto entity = EntityForWrite(engineSlot);
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    CUtlString element, name;
    SetStr(element, elementId);
    SetStr(name, className);
    const int32_t state = on ? ClassPresent : ClassAbsent;

    const Bindings& bindings = _entities.Bindings();
    if (engineSlot == EveryoneSlot)
    {
        if (!bindings.CustomHudSetHasClass)
        {
            return std::unexpected(Error::Unsupported("the custom HUD class setter did not bind"));
        }
        bindings.CustomHudSetHasClass(*entity, &element, &name, state);
        return {};
    }

    if (!bindings.CustomHudSetHasClassForPlayer)
    {
        return std::unexpected(Error::Unsupported("the custom HUD per-player class setter did not bind"));
    }
    bindings.CustomHudSetHasClassForPlayer(*entity, engineSlot, &element, &name, state);
    return {};
}

Status ScreenEntity::SendCursor(int engineSlot, bool shown)
{
    auto entity = EntityForWrite(engineSlot);
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    if (engineSlot != EveryoneSlot)
    {
        const auto& set = _entities.Bindings().CustomHudSetInputCapture;
        if (!set)
        {
            return std::unexpected(Error::Unsupported("the custom HUD input capture setter did not bind"));
        }
        set(*entity, engineSlot, shown);
        return {};
    }

    // No engine setter takes the global state; this plain bool has no container or index behind it.
    Schema::CCSCustomHudLayout{*entity}.GlobalLayoutState().SetInputCaptureEnabled(shown);
    return {};
}

}  // namespace VoltMod
