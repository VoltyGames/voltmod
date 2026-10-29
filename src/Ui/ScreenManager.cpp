#include "Ui/LayoutPath.hpp"
#include "Ui/ScreenEntity.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Ui/ScreenManager.hpp>
#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace VoltMod
{

ScreenManager::ScreenManager(EntitySystem& entities, const Bindings& bindings, SlotEvents& slots,
                             Visibility& visibility, const NetChannels& clients, Connector connect)
    : Pressed({.OnFirst = [this] { return WatchPresses(); }, .OnLast = [this] { _host.Reset(); }}),
      _entities(entities),
      _bindings(bindings),
      _slots(slots),
      _visibility(visibility),
      _clients(clients),
      _connect(std::move(connect))
{}

bool ScreenManager::WatchPresses()
{
    if (!_bindings.ClientFilterTable)
    {
        Log::Warn("Screens: the client filter table did not bind; button presses will not arrive.");
        return false;
    }
    _host = _connect(*this);
    return true;
}

void ScreenManager::OnPress(int slot, std::string_view buttonId)
{
    Pressed.Raise({.Slot = slot, .ButtonId = std::string(buttonId)});
}

Result<Screen> ScreenManager::Create(std::string_view layout, int slot)
{
    if (!IsValidSlot(slot))
    {
        return std::unexpected(Error::Invalid(std::format("slot {} is not a player slot", slot)));
    }

    auto path = LayoutPath::Parse(layout);
    if (!path)
    {
        return std::unexpected(path.error());
    }

    return Screen(
        std::make_unique<ScreenEntity>(_entities, _slots, _visibility, _clients, _live, std::move(*path), slot));
}

bool ScreenManager::HasCursor(int slot) const
{
    return std::ranges::any_of(
        _live, [slot](const ScreenEntity* screen) { return screen->Owner() == slot && screen->HasCursor(); });
}

Status ScreenManager::Available() const
{
    const std::pair<bool, std::string_view> needed[] = {
        {static_cast<bool>(_bindings.CustomHudSetHasClass), "CCSCustomHudLayout::SetHasClass"},
        {static_cast<bool>(_bindings.CustomHudSetDialogVariable), "CCSCustomHudLayout::SetDialogVariableString"},
        {static_cast<bool>(_bindings.CustomHudSetInputCapture), "CCSCustomHudLayout::SetInputCaptureEnabled"},
        {static_cast<bool>(_bindings.ClientFilterTable), "CServerSideClient::INetworkMessageProcessingPreFilter"},
    };
    for (const auto& [bound, key] : needed)
    {
        if (!bound)
        {
            return std::unexpected(Error::Unsupported(std::format("gamedata '{}' did not bind", key)));
        }
    }
    return {};
}

}  // namespace VoltMod
