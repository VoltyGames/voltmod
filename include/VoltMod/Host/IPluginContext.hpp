#pragma once

#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/IPluginAddons.hpp>
#include <VoltMod/Host/IPluginEvents.hpp>
#include <VoltMod/Host/IPluginGameData.hpp>
#include <VoltMod/Host/IPluginLanguages.hpp>
#include <VoltMod/Host/IPluginServices.hpp>
#include <VoltMod/Schema/LiveField.hpp>
#include <cstdint>
#include <string_view>

namespace VoltMod
{

/** One plugin's view of the host. Everything reachable from it is host-owned and outlives the plugin.
 *  Game thread only. */
struct IPluginContext
{
    /** The plugin's name: its manifest name and directory under `addons/voltmod/plugins/`. */
    virtual std::string_view Name() const = 0;
    virtual std::string_view Version() const = 0;

    /** An engine interface by its exact version name, or null. */
    virtual void* EngineInterface(const char* version) const = 0;
    /** A game server interface by its exact version name, or null. */
    virtual void* ServerInterface(const char* version) const = 0;
    /** The absolute `csgo` directory that relative paths resolve against. */
    virtual std::string_view BaseDir() const = 0;
    /** Each module has its own `KHook::__exported__khook`; a plugin seeds it from this before hooking. */
    virtual KHook::IKHook* HookDispatcher() const = 0;

    virtual IPluginEvents& Events() = 0;
    virtual IPluginServices& Services() = 0;
    virtual IPluginLanguages& Languages() = 0;
    virtual IPluginAddons& Addons() = 0;
    /** Null when the host resolved no gamedata. */
    virtual IPluginGameData* GameData() const = 0;

    /** False when another plugin holds @p name, which the host logs naming both. Released with the plugin. */
    virtual bool RegisterCommand(std::string_view name) = 0;
    /** True while any plugin, this one included, or the host holds @p name. */
    virtual bool IsCommandRegistered(std::string_view name) const = 0;

    /** Print one line under this plugin's `logTag`. @p level is a @ref LogLevel. */
    virtual void WriteLog(uint8_t level, std::string_view text) = 0;
    /** Lines below this are dropped; `volt log <name> <level>` changes it. */
    virtual uint8_t MinLogLevel() const = 0;

    /** Where the running server lays out @p field of @p className; Offset -1 when it is gone. */
    virtual Schema::LiveField FindSchemaField(std::string_view className, std::string_view field) const = 0;

protected:
    ~IPluginContext() = default;
};

}  // namespace VoltMod
