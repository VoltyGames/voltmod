#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Players/PlayerManager.hpp>

namespace VoltMod
{

/** The game about to let one player hear another's voice. */
struct VoiceListen
{
    int Receiver = -1;
    int Sender = -1;
    /** Set to keep @ref Receiver from hearing @ref Sender. */
    bool Blocked = false;
};

/**
 * @brief Who hears whose voice, decided each time the game opens a voice channel.
 *
 * The hook installs on the first subscription. Call @ref Refresh after a decision changes, such
 * as a mute or unmute, so it applies without waiting for the game to set the channel again.
 *
 * @code
 * _subs.Add(runtime.Voice.Before += [this](VoltMod::VoiceListen& listen) {
 *     listen.Blocked = IsMuted(listen.Sender);
 * });
 * @endcode
 */
class Voice
{
public:
    /** @p interfaces and @p players must outlive it; the Runtime declares both above. */
    Voice(Interfaces& interfaces, PlayerManager& players);
    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;

private:
    LazyHook _hook;

public:
    Event<VoiceListen&> Before;

    /** Why voice cannot be filtered: IVEngineServer2 is missing. */
    Status Available() const;

    /** Ask again whether each connected player hears @p senderSlot, so @ref Before decides now. */
    void Refresh(int senderSlot);

private:
    Result<Subscription> Install();

    Interfaces& _interfaces;
    PlayerManager& _players;
};

}  // namespace VoltMod
