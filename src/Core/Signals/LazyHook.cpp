#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <utility>

namespace VoltMod
{

LazyHook::LazyHook(std::string name, std::function<Result<Subscription>()> install)
    : _name(std::move(name)), _install(std::move(install))
{}

LazyHook::~LazyHook()
{
    // Never leave a handler pointing into state that is going away.
    if (_listening != 0)
    {
        Log::Error("{}: {} event(s) still had handlers when the hook went away; one may dangle.", _name, _listening);
    }
}

EventLifecycle LazyHook::ForEvent()
{
    return {.OnFirst = [this] { return AddListener(); }, .OnLast = [this] { RemoveListener(); }};
}

bool LazyHook::AddListener()
{
    if (_listening == 0)
    {
        auto hook = _install();
        if (!hook)
        {
            Log::Warn("{}: {}; its handlers will not fire.", _name, hook.error().Detail);
            return false;
        }
        _hook = std::move(*hook);
    }

    ++_listening;
    return true;
}

void LazyHook::RemoveListener()
{
    if (_listening > 0 && --_listening == 0)
    {
        _hook.Reset();
    }
}

}  // namespace VoltMod
