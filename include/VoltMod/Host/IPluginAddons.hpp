#pragma once

#include <cstdint>

namespace VoltMod
{

/** Workshop addons every connecting client must download, shared by all plugins. */
struct IPluginAddons
{
    /** Require @p addonId until @ref Release; 0 when nothing was added, as for id 0 or a listen server. */
    virtual uint64_t Add(uint64_t addonId) = 0;
    virtual void Release(uint64_t token) = 0;

    /** Whether @p slot has every required addon. */
    virtual bool IsReady(int slot) = 0;

protected:
    ~IPluginAddons() = default;
};

}  // namespace VoltMod
