#pragma once

#include "Host/Loading/PluginLoader.hpp"
#include "Host/Plugins/PluginHost.hpp"

#include <VoltMod/Engine/Server/ServerCommand.hpp>
#include <string_view>

namespace VoltMod
{

/** The operator's `volt` console command. Registered before any plugin loads, so a plugin taking
 *  `volt` gets the usual conflict error. */
class VoltCommand
{
public:
    VoltCommand(PluginHost& host, PluginLoader& loader);
    ~VoltCommand();

    VoltCommand(const VoltCommand&) = delete;
    VoltCommand& operator=(const VoltCommand&) = delete;

private:
    void Run(const CCommand& arguments);
    /** The release and the commit the host was built from. */
    void PrintVersion() const;
    void PrintLoaded() const;
    void PrintStatus(std::string_view name);
    /** `volt log <name> <level>`: silence one plugin below @p level. */
    void SetLogLevel(std::string_view name, std::string_view level);

    PluginHost& _host;
    PluginLoader& _loader;
    ServerCommand _command;
};

}  // namespace VoltMod
