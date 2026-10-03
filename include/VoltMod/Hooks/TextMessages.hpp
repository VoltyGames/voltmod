#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <string_view>

namespace VoltMod
{

/** A chat, center or alert line the server is about to send. */
struct TextMessage
{
    /** The line, or the `#token` the client translates, such as `#Player_Point_Award_Killed_Enemy`. */
    std::string_view Text;
    /** Set to send the line to nobody. */
    bool Blocked = false;
};

/**
 * @brief Outgoing TextMsg user messages, the game's own lines included.
 *
 * The hook installs on the first subscription and covers every message posted through
 * IGameEventSystem::PostEventAbstract.
 *
 * @code
 * _subs.Add(runtime.TextMessages.Before += [](VoltMod::TextMessage& text) {
 *     if (text.Text.starts_with("#Player_Point_Award_"))
 *     {
 *         text.Blocked = true;
 *     }
 * });
 * @endcode
 */
class TextMessages
{
public:
    /** @p interfaces must outlive it; the Runtime declares it above. */
    explicit TextMessages(Interfaces& interfaces);
    TextMessages(const TextMessages&) = delete;
    TextMessages& operator=(const TextMessages&) = delete;

private:
    LazyHook _hook;

public:
    Event<TextMessage&> Before;

    /** Why messages cannot be seen: IGameEventSystem is missing. */
    Status Available() const;

private:
    Interfaces& _interfaces;
};

}  // namespace VoltMod
