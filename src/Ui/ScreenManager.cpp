#include "Ui/LayoutPath.hpp"
#include "Ui/ScreenEntity.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Ui/ScreenManager.hpp>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace VoltMod
{

ScreenManager::ScreenManager(EntitySystem& entities, const Bindings& bindings, SlotEvents& slots,
                             Visibility& visibility, Connector connect)
    : Pressed({.OnFirst = [this] { return ListenForPresses(); }, .OnLast = [this] { _host.Reset(); }}),
      _entities(entities),
      _bindings(bindings),
      _slots(slots),
      _visibility(visibility),
      _connect(std::move(connect))
{}

bool ScreenManager::ListenForPresses()
{
    if (!_bindings.FilterMessage || !_bindings.ClientMessageFilter)
    {
        Log::Warn("Screens: the FilterMessage bindings are missing; button presses will not arrive.");
        return false;
    }
    _host = _connect(*this);
    return true;
}

void ScreenManager::OnPress(int slot, std::string_view buttonId)
{
    Pressed.Raise({.Slot = slot, .ButtonId = std::string(buttonId)});
}

Result<Screen> ScreenManager::Shared(std::string_view layout)
{
    return Create(layout, EveryoneSlot);
}

Result<Screen> ScreenManager::ForPlayer(std::string_view layout, int slot)
{
    if (!IsValidSlot(slot))
    {
        return std::unexpected(Error::Invalid(std::format("slot {} is not a player slot", slot)));
    }

    if (auto visible = _visibility.Available(); !visible)
    {
        return std::unexpected(
            Error::Unsupported(std::format("a player screen needs the Visibility filter: {}", visible.error().Detail)));
    }

    return Create(layout, slot);
}

Status ScreenManager::Available() const
{
    const std::pair<bool, std::string_view> needed[] = {
        {static_cast<bool>(_bindings.CustomHudSetHasClass), "CCSCustomHudLayout::SetHasClass"},
        {static_cast<bool>(_bindings.CustomHudSetHasClassForPlayer), "CCSCustomHudLayout::SetHasClassForPlayer"},
        {static_cast<bool>(_bindings.CustomHudSetDialogVariable), "CCSCustomHudLayout::SetDialogVariableString"},
        {static_cast<bool>(_bindings.CustomHudSetDialogVariableForPlayer),
         "CCSCustomHudLayout::SetDialogVariableStringForPlayer"},
        {static_cast<bool>(_bindings.CustomHudSetInputCapture), "CCSCustomHudLayout::SetInputCaptureEnabled"},
        {static_cast<bool>(_bindings.FilterMessage), "INetworkMessageProcessingPreFilter::FilterMessage"},
        {static_cast<bool>(_bindings.ClientMessageFilter), "CServerSideClient::INetworkMessageProcessingPreFilter"},
        {static_cast<bool>(_bindings.ClientSlot), "CServerSideClientBase::m_nClientSlot"},
    };
    for (const auto& [bound, key] : needed)
    {
        if (!bound)
        {
            return std::unexpected(Error::Unsupported(std::format("gamedata '{}' did not bind", key)));
        }
    }
    return _visibility.Available();
}

Result<Screen> ScreenManager::Create(std::string_view layout, int owner)
{
    auto path = LayoutPath::Parse(layout);
    if (!path)
    {
        return std::unexpected(path.error());
    }

    return Screen(std::make_unique<ScreenEntity>(_entities, _slots, _visibility, std::move(*path), owner));
}

}  // namespace VoltMod
