# $title

What the plugin does for players and server operators, in a sentence or two.

## Install

Build and install it into `CS2_SERVER_PATH`:

```bash
uv run poe run $name
```

Run `volt list` in the server console. `$name` should be in the loaded plugin list.

## Commands

| Command | Who | What it does |
| --- | --- | --- |
| `!ping` | Everyone | Check that the plugin is alive |

## Configuration

Settings are in `addons/voltmod/plugins/$name/configs/settings.jsonc` on the server. The file is
seeded on the first install and never overwritten.

| Setting | Default | Purpose |
| --- | --- | --- |
| `plugin.locale` | `en` | Server language: a file in `translations/` |
| `addonId` | `0` | Workshop addon every connecting client must download; 0 requires none |
| `menu.panorama` | `false` | Draw menus with the Panorama layout; players without it get center HTML |

Player-facing text is in `translations/`.

## Menu screen and workshop content

`panorama/screens/$screen.xml.j2` and `.css.j2` are the menu layout; `uv run poe build` renders
them and the header `Ui/${namespace}Menu.hpp`. Models, particles and sounds go in `content/`, compiled with
the CS2 Workshop Tools, and the compiled files the server needs go in `server-assets/`, which the
host mounts while the plugin is loaded. Publish the addon and set its id as `addonId`.

A plugin with no menu or no workshop addon removes those parts by hand:

| Not needed | Remove |
| --- | --- |
| Menu screen | `panorama/`; in `src/App.hpp` the `Ui/` and `PanoramaMenuLayout.hpp` includes, `MenuLayout` and `Panorama`; in `src/App.cpp` the `UsePanorama` block; `menu` in `src/Config.hpp` and `configs/settings.jsonc` |
| Workshop addon | `content/`; `Addon` in `src/App.hpp` and its line in `src/App.cpp`; `addonId` in `src/Config.hpp` and `configs/settings.jsonc` |
