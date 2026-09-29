#pragma once

#include <VoltMod/Engine/EngineTypes.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** The models, particles and sound events a plugin ships, loaded with every map. A path loads from
 *  the next map on, and clients need the file too, such as from a workshop addon. */
class Precache
{
public:
    /** Queues @p path, such as "particles/foo.vpcf". Empty paths and repeats are skipped. */
    void Add(std::string_view path);

    /** Lists every queued path in @p manifest; the framework calls it as the engine builds a map's. */
    void WriteTo(IEntityResourceManifest& manifest) const;

private:
    std::vector<std::string> _resources;
};

}  // namespace VoltMod
