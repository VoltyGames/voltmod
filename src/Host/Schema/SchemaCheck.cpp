#include "Host/Schema/SchemaCheck.hpp"

#include "Core/Files/GameBuild.hpp"
#include "Engine/Memory/SigScanner.hpp"
#include "Host/Schema/SchemaDump.hpp"
#include "Host/Schema/SchemaFields.hpp"
#include "Schema/Layout.hpp"

#include <VoltMod/Core/Files/Paths.hpp>
#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Result.hpp>
#include <entity2/entityclass.h>
#include <entity2/entitysystem.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <gameentitysystem.h>
#include <interfaces/interfaces.h>
#include <schemasystem/schemasystem.h>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod::Schema
{

/** Output consumed by `voltmod framework schemagen`. */
static constexpr std::string_view DumpPath = "addons/voltmod/gamedata/dumps/schema.json";

/** The build stamp near the top of the dump at @p path, or empty. */
static std::string DumpedBuild(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    std::string head(256, '\0');
    file.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<size_t>(file.gcount()));

    constexpr std::string_view key = "\"build\":";
    const size_t at = head.find(key);
    if (at == std::string::npos)
    {
        return {};
    }

    const size_t open = head.find('"', at + key.size());
    const size_t close = open == std::string::npos ? std::string::npos : head.find('"', open + 1);
    return close == std::string::npos ? std::string() : head.substr(open + 1, close - open - 1);
}

static CSchemaSystemTypeScope* ServerScope(ISchemaSystem* schema)
{
    if (!schema || !schema->SchemaSystemIsReady())
    {
        return nullptr;
    }
    const std::string moduleName = PlatformModuleName("server");
    return schema->FindTypeScopeForModule(moduleName.c_str());
}

void WriteSchemaDump(ISchemaSystem* schema, CGameEntitySystem* entities)
{
    static bool attempted = false;  // one try per process; the build stamp prevents repeats
    if (attempted || !entities)
    {
        return;
    }

    CSchemaSystemTypeScope* server = ServerScope(schema);
    const std::filesystem::path output = ResolvePath(DumpPath);
    if (!server || DumpedBuild(output) == GameBuild())
    {
        return;
    }

    // One serializer database covers every networked class.
    const CEntityClass* entityClass = entities->FindClassByName("CBaseEntity");
    const CNetworkSerializerClassInfo* serializer = entityClass ? entityClass->m_NetworkSerializerInfo : nullptr;
    if (!serializer || !serializer->m_pDatabase)
    {
        return;
    }

    attempted = true;
    const Status written =
        WriteDumpFile(schema->GlobalTypeScope(), server, *serializer->m_pDatabase, DumpPath, GameBuild());
    if (!written)
    {
        Log::Warn("Schema: no dump written to {}: {}", output.string(), written.error().Detail);
        return;
    }
    Log::Info("Schema: dumped game build {} to {}.", GameBuild(), output.string());
}

LiveField FindLiveField(ISchemaSystem* schema, std::string_view className, std::string_view field)
{
    CSchemaSystemTypeScope* server = ServerScope(schema);
    if (!server)
    {
        return {};
    }

    const std::string name(className);
    CSchemaClassInfo* live = server->FindDeclaredClass(name.c_str()).Get();
    if (CSchemaSystemTypeScope* global = schema->GlobalTypeScope(); !live && global)
    {
        live = global->FindDeclaredClass(name.c_str()).Get();
    }

    const SchemaClassFieldData_t* found = live ? FindField(live, field) : nullptr;
    if (!found)
    {
        return {};
    }

    int size = 0;
    uint8_t alignment = 0;
    const bool sized = found->m_pType && found->m_pType->GetSizeAndAlignment(size, alignment);
    return {.Offset = found->m_nSingleInheritanceOffset, .Size = sized ? size : 0};
}

}  // namespace VoltMod::Schema

namespace VoltMod
{

SchemaCheck::SchemaCheck(PluginRegistry& registry, ISchemaSystem* schema) : _registry(registry), _schema(schema)
{
    _registry.SetSchemaFields([schema](std::string_view className, std::string_view field) {
        return Schema::FindLiveField(schema, className, field);
    });
    Check();
}

void SchemaCheck::OnServerStartup()
{
    // The first map is the next chance when the schema scope was not there at host load.
    if (!_schemaLoaded)
    {
        Check();
    }
    Schema::WriteSchemaDump(_schema, ::GameEntitySystem());
}

void SchemaCheck::Check()
{
    _schemaLoaded = Schema::ServerScope(_schema) != nullptr;
    if (!_schemaLoaded)
    {
        Log::Error("Schema: the server schema scope is not ready");
        return;
    }

    const Status resolved =
        Schema::ResolveLayout(Schema::GeneratedLayout(), [this](std::string_view className, std::string_view field) {
            return Schema::FindLiveField(_schema, className, field);
        });
    if (!resolved)
    {
        Log::Error("Schema: {}", resolved.error().Detail);
        return;
    }
    Log::Info("Schema: every generated field found on game build {}.", GameBuild());
}

}  // namespace VoltMod
