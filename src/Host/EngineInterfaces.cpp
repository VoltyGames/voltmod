#include "Host/EngineInterfaces.hpp"

#include <VoltMod/Core/Log.hpp>
#include <eiface.h>
#include <filesystem.h>
#include <format>
#include <icvar.h>
#include <interfaces/interfaces.h>
#include <iserver.h>
#include <schemasystem/schemasystem.h>

namespace VoltMod
{

/** Naming the version is the point: a game update is what breaks one. */
template <class Iface>
static Status Find(Iface*& target, InterfaceFactory factory, const char* version)
{
    target = static_cast<Iface*>(factory(version, nullptr));
    if (target == nullptr)
    {
        return std::unexpected(Error::Engine(std::format("could not find interface: {}", version)));
    }
    return {};
}

template <class Iface>
static void FindOptional(Iface*& target, InterfaceFactory factory, const char* version)
{
    if (Status found = Find(target, factory, version); !found)
    {
        Log::Error("{}", found.error().Detail);
    }
}

Result<EngineInterfaces> ResolveEngineInterfaces(const LoaderHandoff& start)
{
    const InterfaceFactory engine = start.EngineFactory;
    const InterfaceFactory server = start.ServerFactory;
    EngineInterfaces found;

    for (const Status& required : {
             Find(found.ServerGameDll, server, INTERFACEVERSION_SERVERGAMEDLL),
             Find(found.ServerGameClients, server, INTERFACEVERSION_SERVERGAMECLIENTS),
             Find(found.NetworkServerService, engine, NETWORKSERVERSERVICE_INTERFACE_VERSION),
             Find(found.GameEntities, server, INTERFACEVERSION_SERVERGAMEENTS),
             Find(found.Cvar, engine, CVAR_INTERFACE_VERSION),
             Find(found.Engine, engine, INTERFACEVERSION_VENGINESERVER),
         })
    {
        if (!required)
        {
            return std::unexpected(required.error());
        }
    }

    FindOptional(found.Schema, engine, SCHEMASYSTEM_INTERFACE_VERSION);
    FindOptional(found.Resources, engine, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
    FindOptional(found.Files, engine, FILESYSTEM_INTERFACE_VERSION);
    return found;
}

}  // namespace VoltMod
