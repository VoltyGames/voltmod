#pragma once

#include "Host/Plugins/CallbackList.hpp"

#include <VoltMod/Host/IHostServices.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** Interfaces plugins publish to each other. Only the publishing owner can refresh or withdraw a name;
 *  publish order is the replay order. */
class ServiceTable
{
public:
    using OwnerId = const void*;

    /** Add or refresh @p name. A peer's live pointer is never swapped out from under it. */
    void Publish(OwnerId owner, std::string_view name, void* implementation);
    void Unpublish(OwnerId owner, std::string_view name);
    void* Find(std::string_view name) const;

    /** Withdraw everything @p owner published and report the names it was still holding. */
    std::vector<std::string> RemoveAll(OwnerId owner);

    /** Hand @p callback what is already published: what a late subscriber is promised. */
    void NotifyPublished(IHostServices::ChangedFn callback, void* context) const;

    CallbackList<IHostServices::ChangedFn>& Changed() { return _changed; }

private:
    struct Service
    {
        std::string Name;
        void* Implementation = nullptr;
        OwnerId Owner = nullptr;
    };

    void RaiseChanged(std::string_view name, bool published);

    std::vector<Service> _services;
    CallbackList<IHostServices::ChangedFn> _changed;
};

}  // namespace VoltMod
