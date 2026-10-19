#include "Menu/CenterHtmlRender.hpp"
#include "Menu/MenuCursor.hpp"

#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Entities/Controller.hpp>
#include <VoltMod/Entities/Pawn.hpp>
#include <VoltMod/Menu/CenterHtmlMenu.hpp>
#include <VoltMod/Messaging/CenterHtml.hpp>
#include <memory>
#include <utility>

namespace VoltMod
{

CenterHtmlMenu::CenterHtmlMenu(const Services& services)
    : _services(services),
      _stack(*this, _services.Translations, services.Scheduler, services.Slots),
      _cursors(services.Slots)
{}

bool CenterHtmlMenu::OpenSession(int slot, std::shared_ptr<Menu> menu, MenuOptions options)
{
    if (!IsValidSlot(slot) || !menu)
    {
        return false;
    }

    CloseAll(slot);

    _services.Freeze.Open(slot, options.FreezeMovement);

    Push(slot, std::move(menu));
    return true;
}

void CenterHtmlMenu::Open(int slot, std::shared_ptr<Menu> menu)
{
    if (!IsValidSlot(slot) || !menu)
    {
        return;
    }

    if (!_stack.IsOpen(slot))
    {
        OpenSession(slot, std::move(menu), {});
        return;
    }

    Push(slot, std::move(menu));
}

void CenterHtmlMenu::Push(int slot, std::shared_ptr<Menu> menu)
{
    _stack.Push(slot, std::move(menu));
    ResetCursor(slot);

    if (!_onFrame)
    {
        _onFrame = _services.Scheduler.EveryFrame([this] { OnGameFrame(); });
    }
}

void CenterHtmlMenu::ResetCursor(int slot)
{
    Cursor& cursor = _cursors[slot];
    cursor.LastSent = {};
    cursor.LastInputTime = Time::MonotonicMs();
    cursor.Selected = MenuCursor::First(MenuCursor::Rows(_stack.Current(slot), slot));
}

void CenterHtmlMenu::Close(int slot)
{
    if (!IsValidSlot(slot))
    {
        return;
    }

    // A prompt of a closing menu must not take later chat.
    _services.ChatInput.CancelCapture(slot);

    if (!_stack.IsOpen(slot))
    {
        return;
    }

    // A parent menu still showing is drawn next frame.
    _stack.Pop(slot);
    if (_stack.IsOpen(slot))
    {
        ResetCursor(slot);
        return;
    }

    _cursors[slot] = {};
    _services.Freeze.Close(slot);
    _services.Messages.ClearCenterHtml(slot);
}

void CenterHtmlMenu::CloseAll(int slot)
{
    if (!IsValidSlot(slot))
    {
        return;
    }

    _services.ChatInput.CancelCapture(slot);

    if (!_stack.IsOpen(slot))
    {
        return;
    }

    _stack.Clear(slot);
    _cursors[slot] = {};
    _services.Freeze.Close(slot);
    _services.Messages.ClearCenterHtml(slot);
}

void CenterHtmlMenu::CloseAll(int slot, std::string_view replyKey)
{
    if (auto& reply = _services.Policy.Reply; reply)
    {
        reply(slot, _services.Translations.Get(std::string(replyKey), slot));
    }

    CloseAll(slot);
}

void CenterHtmlMenu::Prompt(int slot, std::string prompt, std::function<bool(int, std::string_view)> callback)
{
    _services.ChatInput.BeginCapture(slot, std::move(prompt), std::move(callback));
    _cursors[slot].LastSent = {};
}

std::string CenterHtmlMenu::Translate(int slot, std::string_view key, std::string_view fallback) const
{
    return _services.Translations.GetOr(key, slot, fallback);
}

bool CenterHtmlMenu::IsOpen(int slot) const
{
    return _stack.IsOpen(slot);
}

void CenterHtmlMenu::Draw(int slot, bool changed)
{
    Menu* menu = _stack.Current(slot);
    if (!menu)
    {
        return;
    }

    Page& page = _cursors[slot].LastSent;
    const int64_t now = Time::MonotonicMs();
    const bool stale = changed || page.Html.empty() || now - page.RenderedAt >= RenderIntervalMs;
    if (stale)
    {
        std::string html = Render(slot, *menu);
        page.RenderedAt = now;
        if (html != page.Html)
        {
            page.Html = std::move(html);
            page.SentAt = 0;
        }
    }

    // The game also drops a shown page on a death, a respawn or a team switch.
    const Body body = CurrentBody(slot);
    if (body != page.SentTo || now - page.SentAt >= CenterHtmlKeepAliveMs)
    {
        _services.Messages.SendCenterHtml(slot, page.Html);
        page.SentAt = now;
        page.SentTo = body;
    }
}

std::string CenterHtmlMenu::Render(int slot, Menu& menu)
{
    // A pending capture replaces the item list with its prompt.
    if (auto prompt = _services.ChatInput.GetPrompt(slot))
    {
        return RenderCaptureOverlay(menu.Title, *prompt);
    }

    const CenterHtmlView view{
        .Describe = [this, slot](int index) { return _stack.Describe(slot, index); },
        .Breadcrumb = _stack.Breadcrumb(slot),
        .Slot = slot,
        .SelectedIndex = _cursors[slot].Selected,
        .IsSubmenu = _stack.Depth(slot) > 1,
    };
    return RenderMenuHtml(&menu, view, _services.Translations);
}

void CenterHtmlMenu::OnGameFrame()
{
    for (int slot = 0; slot < MaxPlayers; ++slot)
    {
        if (!_stack.IsOpen(slot))
        {
            continue;
        }

        const bool pressed = ReadKeys(slot);

        // Input may have activated a row that closed the menu it was about to draw.
        if (_stack.IsOpen(slot))
        {
            Draw(slot, pressed);
        }
    }

    // Slot reset clears stacks without going through Close, so stop per-frame work here.
    if (!_stack.AnyOpen())
    {
        _onFrame.Reset();
    }
}

CenterHtmlMenu::Body CenterHtmlMenu::CurrentBody(int slot) const
{
    const Controller controller = _services.Entities.Controller(slot);
    const Pawn pawn = controller.Pawn();
    return {.Pawn = pawn.Ref(), .Alive = pawn.IsAlive(), .TeamNum = controller.TeamNum()};
}

}  // namespace VoltMod
