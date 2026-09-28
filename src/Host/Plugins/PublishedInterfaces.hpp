#pragma once

#include <VoltMod/Host/IPluginServices.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** Interfaces plugins publish to each other. Only the publishing owner can refresh or withdraw a name;
 *  publish order is the replay order. */
class PublishedInterfaces
{
public:
    using OwnerId = const void*;

    /** Add or refresh @p name. A peer's live pointer is never swapped out from under it. */
    void Publish(OwnerId owner, std::string_view name, void* implementation);
    void Unpublish(OwnerId owner, std::string_view name);
    void* Find(std::string_view name) const;

    /** Withdraw everything @p owner published and report the names it was still holding. */
    std::vector<std::string> RemoveAll(OwnerId owner);

private:
    struct Service
    {
        std::string Name;
        void* Implementation = nullptr;
        OwnerId Owner = nullptr;
    };

    std::vector<Service> _services;
};

}  // namespace VoltMod
