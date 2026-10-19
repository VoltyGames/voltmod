#include "Menu/CenterHtmlRender.hpp"
#include "Menu/MenuCursor.hpp"

#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Entities/Controller.hpp>
#include <VoltMod/Menu/CenterHtmlMenu.hpp>
#include <cstdint>
#include <in_buttons.h>

namespace VoltMod
{

bool CenterHtmlMenu::ReadKeys(int slot)
{
    if (!_stack.Current(slot))
    {
        return false;
    }

    Cursor& cursor = _cursors[slot];
    const uint64_t buttons = _services.Entities.Controller(slot).Buttons();
    const uint64_t pressed = buttons & ~cursor.PrevButtons;
    cursor.PrevButtons = buttons;

    if (pressed == 0)
    {
        return false;
    }

    const int64_t now = Time::MonotonicMs();
    if (now - cursor.LastInputTime < PressGapMs)
    {
        return false;
    }

    if (_services.ChatInput.IsCapturing(slot))
    {
        if ((pressed & IN_RELOAD) == 0)
        {
            return false;
        }

        _services.ChatInput.CancelCapture(slot);
        cursor.LastInputTime = now;
        return true;
    }

    if (!RunKey(slot, pressed))
    {
        return false;
    }

    cursor.LastInputTime = now;
    return true;
}

bool CenterHtmlMenu::RunKey(int slot, uint64_t pressed)
{
    if (pressed & IN_RELOAD)
    {
        Close(slot);
        return true;
    }

    Menu* menu = _stack.Current(slot);
    const int itemCount = menu ? static_cast<int>(menu->Items.size()) : 0;
    if (itemCount == 0)
    {
        return false;
    }

    if (pressed & IN_FORWARD)
    {
        MoveCursor(slot, -1);
        return true;
    }
    if (pressed & IN_BACK)
    {
        MoveCursor(slot, +1);
        return true;
    }
    if (pressed & (IN_MOVELEFT | IN_MOVERIGHT))
    {
        const int direction = (pressed & IN_MOVELEFT) ? -1 : +1;
        if (_stack.Step(slot, _cursors[slot].Selected, direction))
        {
            return true;
        }
        if (itemCount <= CenterHtmlRowsPerPage)
        {
            return false;
        }
        JumpPage(slot, direction);
        return true;
    }
    if (pressed & IN_USE)
    {
        _stack.Activate(slot, _cursors[slot].Selected);
        return true;
    }
    return false;
}

void CenterHtmlMenu::MoveCursor(int slot, int step)
{
    Select(slot, MenuCursor::Step(MenuCursor::Rows(_stack.Current(slot), slot), _cursors[slot].Selected, step));
}

void CenterHtmlMenu::JumpPage(int slot, int delta)
{
    const CursorRows rows = MenuCursor::Rows(_stack.Current(slot), slot);
    Select(slot, MenuCursor::JumpPage(rows, _cursors[slot].Selected, CenterHtmlRowsPerPage, delta));
}

void CenterHtmlMenu::Select(int slot, int index)
{
    // Ignore stale or client-forged row indexes.
    Menu* menu = _stack.Current(slot);
    if (index < 0 || !menu || index >= static_cast<int>(menu->Items.size()))
    {
        return;
    }

    // Returning to a stepped row keeps its value pending.
    if (!_stack.IsPending(slot, index))
    {
        _stack.ApplyPending(slot);
    }

    _cursors[slot].Selected = index;
}

}  // namespace VoltMod
