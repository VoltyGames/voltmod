#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

namespace VoltMod
{

/**
 * @brief A per-key cooldown, such as one notice per player per minute.
 *
 * A slot key resets when a player rejoins into another slot; a SteamID key does not.
 */
template <class TKey, class TMap = std::unordered_map<TKey, int64_t>>
class Cooldown
{
public:
    Cooldown() = default;
    explicit Cooldown(int64_t seconds) : _seconds(seconds) {}

    /** Starts @p key's cooldown and returns true, or false while it is still running. 0 seconds never blocks. */
    bool TryStart(const TKey& key, int64_t nowSec, int64_t seconds)
    {
        if (SecondsLeft(key, nowSec, seconds) > 0)
        {
            return false;
        }
        _startedAt[key] = nowSec;
        return true;
    }

    bool TryStart(const TKey& key, int64_t nowSec) { return TryStart(key, nowSec, _seconds); }

    /** 0 when @p key is free, also after the clock jumped back. */
    int64_t SecondsLeft(const TKey& key, int64_t nowSec, int64_t seconds) const
    {
        if (seconds <= 0)
        {
            return 0;
        }
        auto it = _startedAt.find(key);
        if (it == _startedAt.end())
        {
            return 0;
        }
        const int64_t elapsed = nowSec - it->second;
        if (elapsed < 0 || elapsed >= seconds)
        {
            return 0;
        }
        return seconds - elapsed;
    }

    int64_t SecondsLeft(const TKey& key, int64_t nowSec) const { return SecondsLeft(key, nowSec, _seconds); }

    /** Starts it without checking, for a caller that already checked. */
    void Start(const TKey& key, int64_t nowSec) { _startedAt[key] = nowSec; }

    void Reset(const TKey& key) { _startedAt.erase(key); }

    /** Removes keys started more than @p maxAgeSec ago. */
    void RemoveExpired(int64_t nowSec, int64_t maxAgeSec)
    {
        std::erase_if(_startedAt, [&](const auto& entry) { return nowSec - entry.second > maxAgeSec; });
    }

private:
    int64_t _seconds = 0;
    TMap _startedAt;
};

/** std::pair has no std::hash, so pair keys use an ordered map. */
template <class TFirst, class TSecond>
using PairCooldown = Cooldown<std::pair<TFirst, TSecond>, std::map<std::pair<TFirst, TSecond>, int64_t>>;

}  // namespace VoltMod
