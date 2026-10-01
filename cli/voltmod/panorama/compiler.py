import shutil
from dataclasses import dataclass
from pathlib import Path

from voltmod import console
from voltmod.panorama.sources import panorama_plugins, rendered_dir
from voltmod.workshop_tools import AddonDirs

# Staged beside the .vtex descriptor that names it; never handed to the compiler.
STAGED_ONLY_SUFFIXES = (".png",)

# The Workshop Manager packs layouts, styles and images only under custom_game.
PANORAMA_DIRS = ("layout/custom_game", "styles/custom_game", "images/custom_game")

# What a screen compiles into.
SCREEN_SUFFIXES = (".xml", ".css", ".vtex", ".svg")


@dataclass(frozen=True, slots=True)
class StagedPlugin:
    name: str
    files: list[Path]

    @property
    def compilable(self) -> list[Path]:
        return [path for path in self.files if path.suffix in SCREEN_SUFFIXES]


def stage(root: Path, names: list[str] | None, dirs: AddonDirs) -> list[StagedPlugin]:
    """Copy each named plugin's rendered screens into the addon's sources."""
    staged = []
    for plugin in panorama_plugins(root, names):
        rendered = rendered_dir(root, plugin)
        if files := _stage_files(rendered, dirs.sources):
            staged.append(StagedPlugin(plugin.name, files))
        else:
            console.note(f"{plugin.name}: nothing rendered under {rendered}")
    return staged


def install_into_client(dirs: AddonDirs, staged: list[StagedPlugin]) -> int:
    """Copy the compiled resources into the client's own csgo/, where a local game loads them."""
    csgo = dirs.client / "game/csgo"
    installed = 0
    for plugin in staged:
        console.section(plugin.name)
        for source in plugin.compilable:
            compiled = dirs.compiled_path(source)
            target = csgo / source.relative_to(dirs.sources).parent / compiled.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(compiled, target)
            console.item(str(target.relative_to(dirs.client)))
            installed += 1
    return installed


def _stage_files(rendered: Path, sources: Path) -> list[Path]:
    """Copy a rendered tree into `sources`, keeping the panorama/ prefix that includes rely on."""
    staged = []
    for source in _rendered_files(rendered):
        target = sources / source.relative_to(rendered.parent)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        staged.append(target)
    return staged


def _rendered_files(rendered: Path) -> list[Path]:
    # Icon sets nest one level deeper than layouts and styles, so this walks rather than globs.
    return [
        path
        for subdir in PANORAMA_DIRS
        for path in sorted((rendered / subdir).rglob("*"))
        if path.is_file() and path.suffix in (*SCREEN_SUFFIXES, *STAGED_ONLY_SUFFIXES)
    ]
