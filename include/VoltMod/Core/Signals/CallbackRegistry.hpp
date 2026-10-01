#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace VoltMod
{

/**
 * @internal
 * @brief Handle-keyed item store behind @ref Event, @ref Scheduler, and @ref GameEvents.
 *
 * Handles start at 1 and never repeat, so 0 means "no registration". Items run in the order they
 * were added.
 */
template <class T>
class CallbackRegistry
{
public:
    /** Store @p item and return its handle. */
    uint64_t Add(T item)
    {
        const uint64_t id = _nextId++;
        _entries.push_back(std::make_unique<Entry>(Entry{.Id = id, .Item = std::move(item)}));
        ++_live;
        return id;
    }

    /** @ref Add, returning a Subscription that removes the item when dropped. */
    [[nodiscard]] Subscription AddOwned(T item)
    {
        const uint64_t id = Add(std::move(item));
        return Subscription([this, id] { Remove(id); });
    }

    /** Whether @p id was stored; an unknown id is ignored. */
    bool Remove(uint64_t id)
    {
        const auto it = Locate(id);
        if (it == _entries.end() || (*it)->Removed)
        {
            return false;
        }

        --_live;
        if (_dispatchDepth > 0)
        {
            // It may be running; it goes when the outermost dispatch ends.
            (*it)->Removed = true;
            _hasRemoved = true;
        }
        else
        {
            _entries.erase(it);
        }
        return true;
    }

    void Clear()
    {
        _live = 0;
        if (_dispatchDepth == 0)
        {
            _entries.clear();
            return;
        }
        for (auto& entry : _entries)
        {
            entry->Removed = true;
        }
        _hasRemoved = true;
    }

    bool Empty() const { return _live == 0; }

    size_t Size() const { return _live; }

    /** Null once removed. */
    T* Find(uint64_t id)
    {
        const auto it = Locate(id);
        return it != _entries.end() && !(*it)->Removed ? &(*it)->Item : nullptr;
    }

    /** Invoke @p fn on every item @p pred accepts. A callback may add or remove items, its own
     *  included, or dispatch again; items it adds first run on the next dispatch. */
    void DispatchIf(std::predicate<const T&> auto&& pred, std::invocable<T&> auto&& fn)
    {
        const size_t count = _entries.size();
        if (count == 0)
        {
            return;
        }

        DispatchScope scope{*this};
        for (size_t i = 0; i < count; ++i)
        {
            // Heap-allocated, so a callback growing the vector leaves it in place.
            Entry& entry = *_entries[i];
            if (!entry.Removed && pred(entry.Item))
            {
                fn(entry.Item);
            }
        }
    }

    /** @ref DispatchIf over every stored item. */
    void Dispatch(std::invocable<T&> auto&& fn)
    {
        DispatchIf([](const T&) { return true; }, std::forward<decltype(fn)>(fn));
    }

private:
    struct Entry
    {
        uint64_t Id = 0;
        T Item;
        bool Removed = false;
    };

    /** Erases removed items once the outermost dispatch ends. */
    struct DispatchScope
    {
        explicit DispatchScope(CallbackRegistry& registry) : Registry(registry) { ++Registry._dispatchDepth; }
        ~DispatchScope()
        {
            if (--Registry._dispatchDepth == 0 && Registry._hasRemoved)
            {
                std::erase_if(Registry._entries, [](const auto& entry) { return entry->Removed; });
                Registry._hasRemoved = false;
            }
        }
        DispatchScope(const DispatchScope&) = delete;
        DispatchScope& operator=(const DispatchScope&) = delete;

        CallbackRegistry& Registry;
    };

    /** Ids only grow, so the entries stay sorted by id. */
    auto Locate(uint64_t id)
    {
        const auto it = std::ranges::lower_bound(_entries, id, {}, [](const auto& entry) { return entry->Id; });
        return it != _entries.end() && (*it)->Id == id ? it : _entries.end();
    }

    std::vector<std::unique_ptr<Entry>> _entries;
    size_t _live = 0;
    int _dispatchDepth = 0;
    bool _hasRemoved = false;
    uint64_t _nextId = 1;
};

}  // namespace VoltMod
