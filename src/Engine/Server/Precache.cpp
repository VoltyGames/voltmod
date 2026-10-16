#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Engine/Server/Precache.hpp>
#include <algorithm>
#include <entity2/entitysystem.h>

namespace VoltMod
{

void Precache::Add(std::string_view path)
{
    if (path.empty())
    {
        return;
    }

    if (std::ranges::find(_resources, path) != _resources.end())
    {
        return;
    }

    _resources.emplace_back(path);
}

void Precache::WriteTo(IEntityResourceManifest& manifest) const
{
    for (const auto& path : _resources)
    {
        manifest.AddResource(path.c_str());
    }

    if (!_resources.empty())
    {
        Log::Info("Precache: added {} resource(s) to the session manifest.", _resources.size());
    }
}

}  // namespace VoltMod
