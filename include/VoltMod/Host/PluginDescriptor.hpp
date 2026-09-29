#pragma once

#include <VoltMod/Host/IPluginContext.hpp>
#include <cstddef>

#if defined(_WIN32)
#define VOLTMOD_EXPORT __declspec(dllexport)
#else
#define VOLTMOD_EXPORT __attribute__((visibility("default")))
#endif

namespace VoltMod
{

/** What a plugin library exports. @ref VoltModVersion stays first: the host reads it before anything else. */
struct PluginDescriptor
{
    /** The VoltMod release the plugin was built with; the host loads only its own. */
    const char* VoltModVersion;

    /** The short commit the plugin was built from. */
    const char* BuildStamp;

    /** Attach to @p host. On false, write why into @p error, which holds @p errorSize bytes. */
    bool (*Load)(IPluginContext* host, char* error, size_t errorSize);

    /** Release everything Load took. The host frees the library only after this returns. */
    void (*Unload)();

    /** This plugin's status as JSON it owns, valid until the next call. */
    const char* (*Status)();
};

/** The one symbol the host resolves in a plugin library. A new name whenever the descriptor's layout
 *  changes, so an older library is never read. */
inline constexpr const char* PluginEntryName = "VoltMod_Plugin";

}  // namespace VoltMod
