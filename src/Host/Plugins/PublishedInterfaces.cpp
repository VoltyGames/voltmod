#include "Host/Plugins/PublishedInterfaces.hpp"

#include <algorithm>
#include <utility>

namespace VoltMod
{

void PublishedInterfaces::Publish(OwnerId owner, std::string_view name, void* implementation)
{
    if (name.empty() || implementation == nullptr)
    {
        return;
    }

    const auto held = std::ranges::find(_services, name, &Service::Name);
    if (held == _services.end())
    {
        _services.push_back({.Name = std::string(name), .Implementation = implementation, .Owner = owner});
    }
    else if (held->Owner == owner)
    {
        held->Implementation = implementation;
    }
}

void PublishedInterfaces::Unpublish(OwnerId owner, std::string_view name)
{
    const auto held = std::ranges::find(_services, name, &Service::Name);
    if (held == _services.end() || held->Owner != owner)
    {
        return;
    }

    _services.erase(held);
}

void* PublishedInterfaces::Find(std::string_view name) const
{
    const auto held = std::ranges::find(_services, name, &Service::Name);
    return held != _services.end() ? held->Implementation : nullptr;
}

std::vector<std::string> PublishedInterfaces::RemoveAll(OwnerId owner)
{
    std::vector<std::string> withdrawn;
    for (const Service& service : _services)
    {
        if (service.Owner == owner)
        {
            withdrawn.push_back(service.Name);
        }
    }

    std::erase_if(_services, [owner](const Service& service) { return service.Owner == owner; });

    return withdrawn;
}

}  // namespace VoltMod
