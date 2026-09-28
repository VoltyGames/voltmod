#pragma once

#include "Config.hpp"

#include <Ui/${namespace}Menu.hpp>
#include <VoltMod/Api.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Signals/Subscriptions.hpp>
#include <VoltMod/Menu/PanoramaMenuLayout.hpp>

namespace $namespace
{

/**
 * Everything this plugin owns for one load cycle. VoltMod destroys it before the runtime, so
 * nothing survives a `volt reload`. Members are destroyed in reverse order.
 */
struct App final : VoltMod::Plugin
{
    explicit App(VoltMod::Runtime& runtime) : Plugin(runtime) {}

    /** Require the addon, draw menus on the layout and register commands. False aborts the plugin load. */
    bool Load() override;

    /** Loaded first, so every member below is built with settings. */
    ConfigManager Config = VoltMod::LoadConfig<ConfigManager>(Runtime);

    VoltMod::PanoramaMenuLayout MenuLayout{Runtime.Screens, ${layout}::Name, ${layout}::Tabs.size(),
                                           ${layout}::Rows.size(), ${layout}::IconSetNames};
    /** Declared after the layout, so it releases first. */
    VoltMod::Subscription Panorama;
    VoltMod::Subscription Addon;

private:
    /** Declared last, so handlers stop before the state they capture goes away. */
    VoltMod::Subscriptions _subs;
};

}  // namespace $namespace
