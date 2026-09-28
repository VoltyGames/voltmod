from enum import StrEnum
from pathlib import Path
from typing import Annotated

import typer

from voltmod import console
from voltmod.content.content import compile_folder, content_dir, export_server_assets, mirror
from voltmod.errors import VoltmodError
from voltmod.options import ServerDir
from voltmod.project import Project
from voltmod.steam import find_client
from voltmod.toolchain.process import WINDOWS
from voltmod.workshop_tools import AddonDirs

Plugin = Annotated[str, typer.Argument(help="The plugin whose content/ holds the sources")]
Addon = Annotated[
    str | None,
    typer.Option("--addon", help="csgo_addons folder name; default: the plugin name"),
]
ClientDir = Annotated[
    Path | None,
    typer.Option("--client", envvar="CS2_CLIENT_PATH", file_okay=False, help="CS2 client root"),
]


class Target(StrEnum):
    CLIENT = "client"
    SERVER = "server"


def compile_command(
    plugin: Plugin,
    folder: Annotated[
        Path, typer.Argument(help="A folder inside content/, such as models/my_plugin/crate")
    ],
    addon: Addon = None,
    install: Annotated[
        list[Target] | None,
        typer.Option("--install", help="Also copy the result loose into this game's csgo/"),
    ] = None,
    prune: Annotated[
        bool, typer.Option("--prune", help="Delete source files nothing references")
    ] = False,
    client: ClientDir = None,
    server: ServerDir = None,
) -> None:
    """Compile one folder of a plugin's content/ with the CS2 Workshop Tools."""
    if not WINDOWS:
        raise VoltmodError("the CS2 Workshop Tools are Windows only")
    root = Project.load().root
    dirs = AddonDirs.of(find_client(client), addon or plugin)
    console.step(f"Compiling {folder.as_posix()} into csgo_addons/{addon or plugin}")
    compiled = compile_folder(content_dir(root, plugin), folder, dirs, prune)

    games = {Target.CLIENT: dirs.client, Target.SERVER: server}
    for target in install or []:
        game = games[target]
        if game is None:
            raise VoltmodError("no CS2 server path; set CS2_SERVER_PATH in .env or pass --server")
        mirror(compiled, game / "game/csgo" / folder)
        console.item(f"installed into the {target}'s game/csgo/{folder.as_posix()}")
    console.done("Compiled")


def server_assets_command(plugin: Plugin, addon: Addon = None, client: ClientDir = None) -> None:
    """Refresh plugins/<plugin>/server-assets/ from the compiled addon; commit the result."""
    root = Project.load().root
    dirs = AddonDirs.of(find_client(client), addon or plugin)
    count = export_server_assets(root, plugin, dirs)
    console.done(f"Copied {count} compiled file(s) into plugins/{plugin}/server-assets")
