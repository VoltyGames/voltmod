#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Host/IPluginAddons.hpp>
#include <cstdint>

namespace VoltMod
{

/**
 * Workshop addons every connecting client must download. The host keeps one list for all plugins, so
 * an addon two plugins require downloads once, one addon per reconnect; see @ref workshop_guide.
 *
 * ```cpp
 * _subs.Add(runtime.AddonManager.Add(Config.Get().addonId));  // 0 requires nothing
 * ```
 */
class MultiAddonManager
{
public:
    explicit MultiAddonManager(IPluginAddons& host) : _host(host) {}

    /** Require @p addonId of every client until the Subscription drops; 0 requires nothing. */
    [[nodiscard]] Subscription Add(uint64_t addonId);

    /** Whether @p slot has every addon any plugin requires. */
    bool IsReady(int slot) const { return _host.IsReady(slot); }

private:
    IPluginAddons& _host;
};

}  // namespace VoltMod
