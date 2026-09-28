# $title

Describe what the plugin does in a sentence or two.

## Getting started

Build the plugin, install it into `CS2_SERVER_PATH` and start the server:

```bash
uv run poe run $name
```

To check it loaded, type `volt list` in the server console and look for `$name`.
Then type `!ping` in chat.

## Commands

| Command | Who can use it | What it does |
| --- | --- | --- |
| `!ping` | Everyone | Replies, to show the plugin is running |

## Settings

The server's copy is `addons/voltmod/plugins/$name/configs/settings.jsonc`. It is created on the
first install, and later installs never overwrite it.

| Setting | Default | What it does |
| --- | --- | --- |
| `plugin.locale` | `en` | Server language, from the files in `translations/` |
| `addonId` | `0` | Workshop addon every player downloads when joining; `0` for none |
| `menu.panorama` | `false` | Show menus with the Panorama screen instead of center text |

## What's in this folder

| Path | What it's for |
| --- | --- |
| `src/` | The plugin's code; `App.cpp` is where it starts |
| `translations/` | Text players see, one file per language |
| `panorama/screens/` | The menu screen; `uv run poe build` turns it into `Ui/${namespace}Menu.hpp` |
| `content/` | Workshop Tools sources: models, particles, sounds |
| `server-assets/` | Add this for compiled files the server itself needs; it installs with the plugin |

## No menu or no addon?

Delete what you don't need:

- **No menu:** delete `panorama/`, then remove `MenuLayout`, `Panorama` and their includes from
  `src/App.hpp`, the `UsePanorama` lines from `src/App.cpp`, and `menu` from `src/Config.hpp` and
  `configs/settings.jsonc`.
- **No addon:** delete `content/`, then remove `Addon` from `src/App.hpp` and `src/App.cpp`, and
  `addonId` from `src/Config.hpp` and `configs/settings.jsonc`.
