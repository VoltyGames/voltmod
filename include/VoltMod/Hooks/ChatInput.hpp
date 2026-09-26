#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace VoltMod
{

/**
 * @brief Per-player pending-prompt registry for menu free-text input.
 *
 * The framework calls @ref TryConsume for every `say`/`say_team` message before
 * command parsing. An active capture routes the message to its callback and
 * suppresses the chat broadcast.
 *
 * This service does not install a separate chat hook: `player_say` runs after the
 * broadcast. The consume call must remain in `Hook_DispatchConCommand`.
 */
class ChatInput
{
public:
    /** Uses @p scheduler for timeouts and @p slots to clear recycled slots. Both must outlive this object. */
    ChatInput(Scheduler& scheduler, SlotEvents& slots);
    ChatInput(const ChatInput&) = delete;
    ChatInput& operator=(const ChatInput&) = delete;

    /** The callback returns true to accept and clear input, or false to keep capturing. */
    using Callback = std::function<bool(int slot, std::string_view text)>;

    /**
     * Begin capturing the next chat line from @p slot. Replaces and cancels any
     * existing capture. A positive @p timeoutMs cancels the capture without input.
     */
    void BeginCapture(int slot, std::string prompt, Callback callback, int timeoutMs = 60000);

    bool IsCapturing(int slot) const;

    /**
     * Route a chat line to the active capture, if any. Returns true when the
     * message was consumed, so the caller must suppress the chat broadcast.
     * A rejected value restores the capture unless the callback installed a replacement.
     */
    bool TryConsume(int slot, std::string_view text);

    /** Cancel without firing the callback. */
    void CancelCapture(int slot);

    /** Returns a copy of the active prompt, or nullopt. The copy remains valid if the capture changes. */
    std::optional<std::string> GetPrompt(int slot) const;

private:
    struct Pending
    {
        std::string Prompt;
        ChatInput::Callback Callback;
        /** Owned by the capture, so replacing or dropping it cancels the timeout. */
        Subscription Timeout;
    };

    Scheduler& _scheduler;
    std::array<std::optional<Pending>, MaxPlayers> _pending{};
    /** Declared after _pending so it unsubscribes before _pending is destroyed. */
    Subscription _slotListener;
};

}  // namespace VoltMod
