#pragma once

#include <string_view>

namespace VoltMod
{

/** Interfaces plugins publish to each other. A pointer lives only while its publisher is loaded, so
 *  look it up where it is used; the host never unloads a plugin mid-dispatch. */
struct IPluginServices
{
    virtual void Publish(std::string_view name, void* implementation) = 0;
    virtual void Unpublish(std::string_view name) = 0;
    virtual void* Find(std::string_view name) = 0;

protected:
    ~IPluginServices() = default;
};

}  // namespace VoltMod
