#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/PerSlot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Core/Text/Translations.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Menu/ChatInput.hpp>
#include <VoltMod/Menu/MenuFreeze.hpp>
#include <VoltMod/Menu/MenuModel.hpp>
#include <VoltMod/Menu/MenuStack.hpp>
#include <VoltMod/Messaging/Messages.hpp>
#include <VoltMod/Players/Policy.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace VoltMod
{

/**
 * @brief Menus drawn as center HTML and driven with W/S/A/D/E/R. It needs no client addon, so
 * `runtime.Menus` falls back to it for a player another surface cannot draw for.
 */
class CenterHtmlMenu final : public MenuSurface
{
public:
    /** All must outlive the menu. */
    struct Services
    {
        VoltMod::Scheduler& Scheduler;
        SlotEvents& Slots;
        EntitySystem& Entities;
        VoltMod::MenuFreeze& Freeze;
        VoltMod::ChatInput& ChatInput;
        VoltMod::Translations& Translations;
        VoltMod::Policy& Policy;
        VoltMod::Messages& Messages;
    };

    explicit CenterHtmlMenu(const Services& services);

    bool OpenSession(int slot, std::shared_ptr<Menu> menu, MenuOptions options) override;
    void Open(int slot, std::shared_ptr<Menu> menu) override;
    bool IsOpen(int slot) const override;
    void Close(int slot) override;
    void CloseAll(int slot) override;
    void CloseAll(int slot, std::string_view replyKey) override;
    void Prompt(int slot, std::string prompt, std::function<bool(int slot, std::string_view text)> callback) override;
    std::string Translate(int slot, std::string_view key, std::string_view fallback) const override;

private:
    /** Presses closer together than this are ignored. */
    static constexpr int64_t PressGapMs = 200;
    /** Rows can show live values, so an idle page is rebuilt this often. */
    static constexpr int64_t RenderIntervalMs = 250;
    /** The game drops center HTML on death or a team switch, so an idle page is re-sent this often. */
    static constexpr int64_t ResendIntervalMs = 100;

    /** One player's place in the menu on top. */
    struct Cursor
    {
        int Selected = 0;
        uint64_t PrevButtons = 0;
        int64_t LastInputTime = 0;
    };

    /** Push @p menu onto an open session and start the per-frame work. */
    void Push(int slot, std::shared_ptr<Menu> menu);

    /** Put the cursor back on the first selectable row of whatever is now on top. */
    void ResetCursor(int slot);

    /** Move @p slot's cursor to row @p index, applying the value pending on the row it leaves. */
    void Select(int slot, int index);

    /** What one player was last sent. */
    struct Page
    {
        std::string Html;
        int64_t RenderedAt = 0;
        int64_t SentAt = 0;
    };

    /** Send the menu on top, or its chat prompt. Rebuilt when @p changed; sent when it differs or
     *  the last send is old. */
    void Draw(int slot, bool changed);
    std::string Render(int slot, Menu& menu);

    void OnGameFrame();

    /** The W/S/A/D/E/R controls for @p slot. True when a press was consumed. */
    bool ReadKeys(int slot);
    bool RunKey(int slot, uint64_t pressed);
    void MoveCursor(int slot, int step);
    void JumpPage(int slot, int delta);

    Services _services;
    /** What every menu surface shares. */
    MenuStack _stack;
    PerSlot<Cursor> _cursors;
    PerSlot<Page> _pages;
    /** Declared last: per-frame delivery drops before the state it touches. */
    Subscription _onFrame;
};

}  // namespace VoltMod
