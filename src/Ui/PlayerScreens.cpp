#include "Ui/LayoutPath.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Ui/PlayerScreens.hpp>
#include <string_view>
#include <utility>

namespace VoltMod
{

/** The root element id of @p layout; a layout @ref ScreenManager::Create refuses never shows anyway. */
static std::string RootElement(std::string_view layout)
{
    auto path = LayoutPath::Parse(layout);
    return path ? std::string(path->Name()) : std::string(layout);
}

PlayerScreens::PlayerScreens(ScreenManager& screens, std::string layout)
    : _screens(screens), _layout(std::move(layout)), _root(RootElement(_layout))
{}

Screen& PlayerScreens::For(int slot)
{
    if (!IsValidSlot(slot))
    {
        return _empty;
    }

    std::optional<Screen>& screen = _created[slot];
    if (!screen)
    {
        auto created = _screens.Create(_layout, slot);
        // Kept empty rather than retried: what Create refuses for lasts the whole load.
        if (!created)
        {
            Log::Warn("Screen '{}': no player screen for slot {} ({}).", _layout, slot, created.error().Detail);
        }
        screen = created ? std::move(*created) : Screen();
    }
    // Gone with its slot's last player or the last map: whatever spawns next starts closed.
    if (!*screen)
    {
        _open[slot] = false;
    }
    return *screen;
}

Screen* PlayerScreens::Find(int slot)
{
    if (!IsValidSlot(slot) || !_created[slot])
    {
        return nullptr;
    }
    return &*_created[slot];
}

Screen* PlayerScreens::FindSpawned(int slot)
{
    if (!_screens.HasClient(slot))
    {
        return nullptr;
    }
    Screen& screen = For(slot);
    return screen.EnsureSpawned(slot) ? &screen : nullptr;
}

Screen* PlayerScreens::Open(int slot)
{
    Screen* screen = FindSpawned(slot);
    if (!screen)
    {
        return nullptr;
    }
    screen->SetHidden(slot, _root, false);
    screen->ShowCursor(slot, true);
    _open[slot] = true;
    return screen;
}

void PlayerScreens::Close(int slot)
{
    if (!IsValidSlot(slot))
    {
        return;
    }
    _open[slot] = false;
    Screen* screen = Find(slot);
    if (!screen || !*screen)
    {
        return;
    }
    screen->ShowCursor(slot, false);
    screen->Remove();
}

bool PlayerScreens::IsOpen(int slot) const
{
    return IsValidSlot(slot) && _open[slot] && _created[slot] && *_created[slot];
}

}  // namespace VoltMod
