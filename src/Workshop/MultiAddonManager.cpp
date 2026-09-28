#include <VoltMod/Workshop/MultiAddonManager.hpp>

namespace VoltMod
{

Subscription MultiAddonManager::Add(uint64_t addonId)
{
    const uint64_t token = _host.Add(addonId);
    if (token == 0)
    {
        return {};
    }
    return Subscription([&host = _host, token] { host.Release(token); });
}

}  // namespace VoltMod
