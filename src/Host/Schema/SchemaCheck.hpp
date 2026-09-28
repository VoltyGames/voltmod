#pragma once

#include "Host/Plugins/PluginRegistry.hpp"

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/IPluginGameData.hpp>

namespace VoltMod
{

/**
 * Compares the baked schema offsets with the live game once per process. The answer is recorded on
 * @ref PluginRegistry and covers only plugins carrying the same layout stamp.
 */
class SchemaCheck
{
public:
    /** Compare the layout once and record the answer on @p registry. Null interfaces check and dump nothing. */
    SchemaCheck(PluginRegistry& registry, ISchemaSystem* schema, IGameResourceService* resources);

    /** Write the dump once a map's entities exist: at most once per process, never over a dump of this build. */
    void OnServerStartup();

private:
    void Check();

    /** The entity system the engine caches in the resource service, null before the first map. */
    CGameEntitySystem* Entities() const;

    PluginRegistry& _registry;
    ISchemaSystem* _schema = nullptr;
    IGameResourceService* _resources = nullptr;
    int _entitySystemOffset = -1;
    bool _schemaLoaded = false;  ///< false while the schema scope did not exist to compare against
};

namespace Schema
{

/**
 * Compare @ref GeneratedLayout with the live schema. Stale offsets hit wrong addresses, so a plugin
 * built from this layout refuses to load when it fails.
 * @return Every mismatch in one message, or ErrorCode::NotReady before the server scope exists.
 */
Status VerifySchemaLayout(ISchemaSystem* schema);

/**
 * Write `addons/voltmod/schema/server.json` for `voltmod framework schemagen` unless it matches this build.
 * Null @p entities (before the first map) writes nothing: the networked flags need its serializers.
 */
void WriteSchemaDump(ISchemaSystem* schema, CGameEntitySystem* entities);

}  // namespace Schema
}  // namespace VoltMod
