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

Result<CEntityInstance*> ScreenEntity::ResolveWritable() const
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

    return entity.Raw();
}

Status ScreenEntity::SendText(std::string_view variable, std::string_view value)
{
    auto entity = ResolveWritable();
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    const auto& set = _entities.Bindings().CustomHudSetDialogVariable;
    if (!set)
    {
        return std::unexpected(Error::Unsupported("the custom HUD text setter did not bind"));
    }

    CUtlString root, name, text;
    SetStr(root, _layout.Name());
    SetStr(name, variable);
    SetStr(text, value);
    set(*entity, &root, &name, &text);
    return {};
}

Status ScreenEntity::SendClass(std::string_view elementId, std::string_view className, bool on)
{
    auto entity = ResolveWritable();
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    const auto& set = _entities.Bindings().CustomHudSetHasClass;
    if (!set)
    {
        return std::unexpected(Error::Unsupported("the custom HUD class setter did not bind"));
    }

    CUtlString element, name;
    SetStr(element, elementId);
    SetStr(name, className);
    set(*entity, &element, &name, on ? ClassPresent : ClassAbsent);
    return {};
}

Status ScreenEntity::SendCursor(bool shown)
{
    auto entity = ResolveWritable();
    if (!entity)
    {
        return std::unexpected(entity.error());
    }

    // The engine's per-player setter returns silently for a slot past the state count.
    if (const int states = PlayerStateCount(); states <= _owner)
    {
        return std::unexpected(
            Error::Failed(std::format("slot {} has no per-player layout state (the entity holds {})", _owner, states)));
    }

    const auto& set = _entities.Bindings().CustomHudSetInputCapture;
    if (!set)
    {
        return std::unexpected(Error::Unsupported("the custom HUD input capture setter did not bind"));
    }
    set(*entity, _owner, shown);
    return {};
}

}  // namespace VoltMod
