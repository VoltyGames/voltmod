#pragma once

#include <cstdint>
#include <string_view>

namespace VoltMod
{

/** Interfaces plugins publish to each other. A pointer lives only while its publisher is loaded, so
 *  look it up where it is used; the host never unloads a plugin mid-dispatch. */
struct IHostServices
{
    /** @p published is false when the name was withdrawn. */
    using ChangedFn = void (*)(void* context, std::string_view name, bool published);

    virtual void Publish(std::string_view name, void* implementation) = 0;
    virtual void Unpublish(std::string_view name) = 0;
    virtual void* Find(std::string_view name) = 0;

    /** Also replays what is already published when the subscription is made. */
    virtual uint64_t OnChanged(ChangedFn callback, void* context) = 0;
    virtual void Unsubscribe(uint64_t token) = 0;

protected:
    ~IHostServices() = default;
};

}  // namespace VoltMod
