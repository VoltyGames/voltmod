#include <VoltMod/Workshop/MultiAddonManager.hpp>

namespace VoltMod
{

Subscription MultiAddonManager::Add(uint64_t addonId)
{
    if (!_host.Add(addonId))
    {
        return {};
    }
    return Subscription([&host = _host, addonId] { host.Remove(addonId); });
}

}  // namespace VoltMod
