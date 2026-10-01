#pragma once

#include "Host/LoaderHandoff.hpp"

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>

namespace VoltMod
{

/** The engine interfaces the host uses, looked up once at start. */
struct EngineInterfaces
{
    ISource2Server* ServerGameDll = nullptr;
    ISource2GameClients* ServerGameClients = nullptr;
    INetworkServerService* NetworkServerService = nullptr;
    ISource2GameEntities* GameEntities = nullptr;
    ICvar* Cvar = nullptr;
    IVEngineServer2* Engine = nullptr;

    // Optional: null when missing, and what needs them says so.
    ISchemaSystem* Schema = nullptr;
    IGameResourceService* Resources = nullptr;
    IFileSystem* Files = nullptr;
    INetworkMessages* NetworkMessages = nullptr;
};

/** Fails when an interface the engine hooks need is missing; the optional ones are logged. */
Result<EngineInterfaces> ResolveEngineInterfaces(const LoaderHandoff& start);

}  // namespace VoltMod
