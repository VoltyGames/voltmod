#pragma once

#include "Host/Plugins/PluginRegistry.hpp"

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Schema/LiveField.hpp>
#include <string_view>

namespace VoltMod
{

/**
 * Fills the host's own generated offsets from the live schema, and gives @ref PluginRegistry the
 * lookup each plugin fills its own from.
 */
class SchemaCheck
{
public:
    /** Resolve the host's layout and hand @p registry the lookup. A null @p schema resolves and dumps nothing. */
    SchemaCheck(PluginRegistry& registry, ISchemaSystem* schema);

    /** Write the dump once a map's entities exist: at most once per process, never over a dump of this build. */
    void OnServerStartup();

private:
    void Check();

    PluginRegistry& _registry;
    ISchemaSystem* _schema = nullptr;
    bool _schemaLoaded = false;  ///< false while the schema scope did not exist to resolve against
};

namespace Schema
{

/** Where the live server schema lays out @p field of @p className or its bases; Offset -1 when gone. */
LiveField FindLiveField(ISchemaSystem* schema, std::string_view className, std::string_view field);

/**
 * Write `addons/voltmod/gamedata/dumps/schema.json` for `voltmod framework schemagen` unless it matches this build.
 * Null @p entities (before the first map) writes nothing: the networked flags need its serializers.
 */
void WriteSchemaDump(ISchemaSystem* schema, CGameEntitySystem* entities);

}  // namespace Schema
}  // namespace VoltMod
