#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/PlayerInput.hpp>
#include <functional>
#include <optional>

namespace VoltMod
{

/**
 * @brief Each player's command from CCSPlayer_MovementServices::RunCommand.
 *
 * The host hooks RunCommand once for every plugin and decodes each command once; this service
 * listens to it while one of its events has a handler. @ref PlayerInput::Valid is false when the
 * usercmd payload was missing.
 */
class Movement
{
public:
    /** Subscribes a Movement to the host's commands; dropping the result unsubscribes. */
    using Connector = std::function<Subscription(Movement&)>;

    /** @p bindings must outlive this; @p connect runs when the first handler arrives. */
    Movement(const Bindings& bindings, Connector connect);
    Movement(const Movement&) = delete;
    Movement& operator=(const Movement&) = delete;

private:
    LazyHook _hook;

public:
    /** Edit this plugin's copy of the command before @ref Before sees it. The engine's usercmd is
     *  unchanged. */
    Event<int, PlayerInput&> Rewrite;
    Event<int, const PlayerInput&> Before;
    Event<int, const PlayerInput&> After;

    /** Why movement events cannot fire: the RunCommand slot or the usercmd offset did not bind. */
    Status Available() const;

    /** @internal The host's command, before and after the engine runs it. */
    void BeforeCommand(int slot, const PlayerInput& input);
    void AfterCommand(int slot, const PlayerInput& input);

private:
    Result<Subscription> Install();

    const Bindings& _bindings;
    Connector _connect;
    std::optional<PlayerInput> _rewritten;  ///< this plugin's copy while Rewrite has handlers
};

}  // namespace VoltMod
