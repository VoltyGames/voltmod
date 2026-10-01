#pragma once

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Host/IPluginEvents.hpp>
#include <cstdint>
#include <exception>

namespace VoltMod::Internal
{

/** The C function the host calls for @p Member, with its owner as the context. Nothing may unwind
 *  into the host. */
template <auto Member>
struct HostCallback;

template <class Owner, class Result, class... Args, Result (Owner::*Member)(Args...)>
struct HostCallback<Member>
{
    static Result Call(void* owner, Args... args) noexcept
    {
        try
        {
            return (static_cast<Owner*>(owner)->*Member)(args...);
        }
        catch (const std::exception& error)
        {
            Log::Error("Unhandled exception in a host event: {}", error.what());
            return Result();
        }
        catch (...)
        {
            Log::Error("Unhandled non-standard exception in a host event.");
            return Result();
        }
    }
};

/** Unsubscribes host event @p token when dropped; @p events must outlive it. */
inline Subscription HostSubscription(IPluginEvents& events, uint64_t token)
{
    return Subscription([&events, token] { events.Unsubscribe(token); });
}

}  // namespace VoltMod::Internal
