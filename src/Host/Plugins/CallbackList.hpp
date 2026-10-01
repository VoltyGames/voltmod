#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace VoltMod
{

/**
 * @internal
 * @brief One host event's callbacks, safe to edit from inside a dispatch.
 *
 * Unlike Core's CallbackRegistry: plain function pointers across the module boundary, ordered by
 * plugin load position then subscription order, and a pass stops at the first that returns true.
 * During a pass a removal only marks and an addition waits aside; both land when the outermost pass ends.
 */
template <class Fn>
class CallbackList
{
public:
    void Add(uint64_t token, uint64_t order, Fn callback, void* context)
    {
        const Entry entry{.Token = token, .Order = order, .Call = callback, .Context = context};
        if (_depth > 0)
        {
            _pending.push_back(entry);
        }
        else
        {
            Insert(entry);
        }
    }

    /** Whether nothing is subscribed, counting additions still waiting aside. */
    bool Empty() const
    {
        return _pending.empty() && std::ranges::all_of(_entries, &Entry::Removed);
    }

    /** Whether @p token was taken on this event. */
    bool Remove(uint64_t token)
    {
        for (Entry& entry : _entries)
        {
            if (entry.Token != token || entry.Removed)
            {
                continue;
            }

            entry.Removed = true;
            if (_depth == 0)
            {
                ApplyPending();
            }
            return true;
        }

        const auto waiting = std::ranges::find(_pending, token, &Entry::Token);
        if (waiting == _pending.end())
        {
            return false;
        }

        _pending.erase(waiting);
        return true;
    }

    /** Invoke @p visit(callback, context) until one returns true; returns whether one did. */
    template <class Visit>
    bool Dispatch(Visit&& visit)
    {
        return Walk([&](const Entry& entry) { return visit(entry.Call, entry.Context); });
    }

    /** Invoke @p visit(callback, context) over one plugin's callbacks: those added with @p order. */
    template <class Visit>
    void DispatchTo(uint64_t order, Visit&& visit)
    {
        Walk([&](const Entry& entry) {
            if (entry.Order == order)
            {
                visit(entry.Call, entry.Context);
            }
            return false;
        });
    }

private:
    struct Entry
    {
        uint64_t Token = 0;
        uint64_t Order = 0;  ///< the owning plugin's load position
        Fn Call = nullptr;
        void* Context = nullptr;
        bool Removed = false;
    };

    template <class Visit>
    bool Walk(Visit&& visit)
    {
        ++_depth;
        bool stopped = false;
        // Nothing is inserted or erased while the depth is up, so the count and the references hold.
        const size_t count = _entries.size();
        for (size_t i = 0; i < count && !stopped; ++i)
        {
            const Entry& entry = _entries[i];
            if (!entry.Removed)
            {
                stopped = visit(entry);
            }
        }
        if (--_depth == 0)
        {
            ApplyPending();
        }
        return stopped;
    }

    void Insert(const Entry& entry)
    {
        const auto at = std::ranges::find_if(_entries, [&](const Entry& held) { return held.Order > entry.Order; });
        _entries.insert(at, entry);
    }

    void ApplyPending()
    {
        std::erase_if(_entries, [](const Entry& entry) { return entry.Removed; });
        for (const Entry& entry : _pending)
        {
            Insert(entry);
        }
        _pending.clear();
    }

    std::vector<Entry> _entries;
    std::vector<Entry> _pending;
    int _depth = 0;
};

}  // namespace VoltMod
