#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <functional>
#include <string>

namespace VoltMod
{

/**
 * @brief An engine hook that exists only while one of its events has handlers.
 *
 * The first event to gain a handler calls @p install and keeps the hook it returns; the last one to
 * go quiet removes it. An install error is logged under @p name and refuses that subscription, so a
 * later subscriber retries.
 *
 * @code
 * Damage::Damage(...)
 *     : _hook("Damage", [this] { return HookFunction("Damage", _bindings.TakeDamage, ...); }),
 *       Before(_hook.ForEvent())
 * @endcode
 *
 * Adapters borrow this object, so declare it before the events they belong to. Destroying it with
 * handlers still attached is an ownership error that can leave a hook in an unloading module; the
 * destructor logs it.
 */
class LazyHook
{
public:
    LazyHook(std::string name, std::function<Result<Subscription>()> install);
    ~LazyHook();

    LazyHook(const LazyHook&) = delete;
    LazyHook& operator=(const LazyHook&) = delete;

    EventLifecycle ForEvent();

    bool Installed() const noexcept { return static_cast<bool>(_hook); }

    /** Number of adapted events with at least one handler. For diagnostics and tests only. */
    int ListeningEvents() const noexcept { return _listening; }

private:
    bool AddListener();
    void RemoveListener();

    std::string _name;
    std::function<Result<Subscription>()> _install;
    Subscription _hook;
    int _listening = 0;
};

}  // namespace VoltMod
