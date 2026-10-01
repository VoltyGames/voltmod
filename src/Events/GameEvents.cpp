#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Engine/Memory/MemoryAccess.hpp>
#include <VoltMod/Events/GameEvents.hpp>
#include <bit>
#include <cstdint>
#include <playerslot.h>

namespace VoltMod
{

GameEvents::GameEvents(Interfaces& interfaces, const Bindings& bindings) : _interfaces(interfaces), _bindings(bindings)
{
    if (_bindings.LegacyGameEventListener)
    {
        _getLegacyListener = std::bit_cast<GetLegacyGameEventListenerFn>(_bindings.LegacyGameEventListener.Ptr());
    }
    else
    {
        Log::Warn("LegacyGameEventListener signature not found; per-client event delivery unavailable.");
    }

    if (_bindings.GameEventManager)
    {
        _interfaces.GameEventManager = ReadAt<IGameEventManager2*>(_bindings.GameEventManager.Ptr(), 0);
    }
}

GameEvents::~GameEvents()
{
    // Remove the listener before destroying it.
    RemoveAllListeners();
}

Status GameEvents::Available() const
{
    if (!_bindings.GameEventManager)
    {
        return std::unexpected(Error::Unsupported("the GameEventManager address did not bind"));
    }
    if (!_interfaces.GameEventManager)
    {
        return std::unexpected(Error::Engine("the game event manager pointer is null"));
    }
    return {};
}

IGameEventListener2* GameEvents::GetClientLegacyListener(int slot) const
{
    if (!_getLegacyListener || !IsValidSlot(slot))
    {
        return nullptr;
    }

    return _getLegacyListener(CPlayerSlot(slot));
}

bool GameEvents::ClientListensTo(int slot, std::string_view eventName) const
{
    auto* mgr = _interfaces.GameEventManager;
    auto* listener = GetClientLegacyListener(slot);
    if (!mgr || !listener || eventName.empty())
    {
        return false;
    }

    return mgr->FindListener(listener, std::string(eventName).c_str());
}

IGameEvent* GameEvents::CreateEvent(std::string_view name)
{
    auto* mgr = _interfaces.GameEventManager;
    if (!mgr || name.empty())
    {
        return nullptr;
    }

    // The manager resolves the descriptor during the call and keeps no pointer.
    return mgr->CreateEvent(std::string(name).c_str());
}

bool GameEvents::FireEvent(IGameEvent* event, bool broadcast)
{
    auto* mgr = _interfaces.GameEventManager;
    if (!mgr || !event)
    {
        return false;
    }

    return mgr->FireEvent(event, !broadcast);
}

void GameEvents::FreeEvent(IGameEvent* event)
{
    auto* mgr = _interfaces.GameEventManager;
    if (mgr && event)
    {
        mgr->FreeEvent(event);
    }
}

Subscription GameEvents::Add(std::string_view eventName, EventCallback callback)
{
    auto* mgr = _interfaces.GameEventManager;
    if (!mgr)
    {
        return {};
    }

    auto it = _events.find(eventName);
    if (it == _events.end())
    {
        it = _events.emplace(std::string(eventName), EventHandlers{}).first;
    }

    // The engine drops this late attachment at the next map startup, where it is re-attached.
    EventHandlers& entry = it->second;
    if (!entry.Attached)
    {
        entry.Attached = mgr->AddListener(this, it->first.c_str(), true);
    }

    return entry.Handlers.AddOwned(std::move(callback));
}

void GameEvents::OnServerStartup()
{
    auto* mgr = _interfaces.GameEventManager;
    if (!mgr || _events.empty())
    {
        return;
    }

    // Detach first, so a surviving listener is not added twice. The engine has no per-event
    // detach, so an event whose handlers all dropped stops here.
    mgr->RemoveListener(this);

    int wanted = 0;
    int attached = 0;
    for (auto& [name, entry] : _events)
    {
        entry.Attached = false;
        if (entry.Handlers.Empty())
        {
            continue;
        }
        ++wanted;
        entry.Attached = mgr->AddListener(this, name.c_str(), true);
        if (entry.Attached)
        {
            ++attached;
        }
        else
        {
            Log::Warn("Game event listener failed to attach: {}.", name);
        }
    }
    Log::Info("Attached {}/{} game event listener(s) at map start.", attached, wanted);
}

void GameEvents::RemoveAllListeners()
{
    // Both Runtime and the destructor call this, so it must be idempotent.
    if (auto* mgr = _interfaces.GameEventManager; mgr && !_events.empty())
    {
        mgr->RemoveListener(this);
    }

    for (auto& [name, entry] : _events)
    {
        entry.Handlers.Clear();
        entry.Attached = false;
    }
}

void GameEvents::FireGameEvent(IGameEvent* event)
{
    if (!event)
    {
        return;
    }

    const char* eventName = event->GetName();
    if (!eventName)
    {
        return;
    }

    const auto it = _events.find(std::string_view(eventName));
    if (it != _events.end())
    {
        it->second.Handlers.Dispatch([event](EventCallback& callback) { callback(event); });
    }
}

}  // namespace VoltMod
