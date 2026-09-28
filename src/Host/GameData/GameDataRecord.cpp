#include "Host/GameData/GameDataRecord.hpp"

#include "Core/Files/GameBuild.hpp"
#include "Host/GameData/GameDataDocument.hpp"

#include <VoltMod/Core/Files/File.hpp>
#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Text/Json.hpp>
#include <format>

template <>
struct glz::meta<VoltMod::GameDataRecord::Location>
{
    using T = VoltMod::GameDataRecord::Location;
    static constexpr auto value = glz::object("module", &T::Module, "rva", &T::Rva);
};

template <>
struct glz::meta<VoltMod::GameDataRecord::Slot>
{
    using T = VoltMod::GameDataRecord::Slot;
    static constexpr auto value =
        glz::object("module", &T::Module, "table", &T::Table, "index", &T::Index, "code", &T::Code);
};

template <>
struct glz::meta<VoltMod::GameDataRecord>
{
    using T = VoltMod::GameDataRecord;
    static constexpr auto value = glz::object("build", &T::Build, "functions", &T::Functions, "globals", &T::Globals,
                                              "vtables", &T::VTables, "offsets", &T::Offsets);
};

namespace VoltMod
{

void WriteGameDataRecord(const GameDataRecord& resolved)
{
    const std::string path = std::format("addons/voltmod/gamedata/resolved.{}.json", PlatformName);
    const std::string build(GameBuild());
    if (const auto existing = Json::ReadFile<GameDataRecord>(path); existing && existing->Build == build)
    {
        return;
    }

    GameDataRecord stamped = resolved;
    stamped.Build = build;
    if (const Status written = WriteAllText(path, Json::WritePretty(stamped)); !written)
    {
        Log::Warn("GameData: no record written to {}: {}", path, written.error().Detail);
    }
    else
    {
        Log::Info("GameData: recorded what resolved on server {} in {}.", build, path);
    }
}

}  // namespace VoltMod
