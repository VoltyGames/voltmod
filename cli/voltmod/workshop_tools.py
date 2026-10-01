"""The CS2 Workshop Tools: an addon's source and compiled folders, and resourcecompiler."""

from dataclasses import dataclass
from pathlib import Path

from voltmod import console
from voltmod.errors import VoltmodError
from voltmod.platforms import Platform
from voltmod.toolchain.process import run

RESOURCE_COMPILER = f"game/bin/{Platform.WINDOWS.bin_dir}/resourcecompiler.exe"

# Source suffix -> what resourcecompiler writes for it.
COMPILED_SUFFIX = {
    ".xml": ".vxml_c",
    ".css": ".vcss_c",
    ".vtex": ".vtex_c",
    ".vmdl": ".vmdl_c",
    ".vpcf": ".vpcf_c",
    ".vsndevts": ".vsndevts_c",
    ".vdata": ".vdata_c",
    ".svg": ".vsvg_c",
}


@dataclass(frozen=True, slots=True)
class AddonDirs:
    """Where the Workshop Tools read one addon's sources and write its compiled resources."""

    client: Path
    sources: Path
    compiled: Path

    @classmethod
    def of(cls, client: Path, addon: str) -> AddonDirs:
        return cls(
            client, client / "content/csgo_addons" / addon, client / "game/csgo_addons" / addon
        )

    def compiled_path(self, source: Path) -> Path:
        relative = source.relative_to(self.sources)
        return (self.compiled / relative).with_suffix(COMPILED_SUFFIX[source.suffix])


def compile_resources(dirs: AddonDirs, sources: list[Path]) -> None:
    """Compile `sources`, all under `dirs.sources`, in one resourcecompiler launch."""
    compiler = dirs.client / RESOURCE_COMPILER
    if not compiler.is_file():
        raise VoltmodError(
            f"CS2 Workshop Tools not found at {compiler}\n"
            "Install them from Steam: Library > Tools > Counter-Strike 2 Workshop Tools."
        )
    if not sources:
        return

    # The tools only treat a directory with addoninfo.txt as an addon.
    info = dirs.compiled / "addoninfo.txt"
    if not info.is_file():
        info.parent.mkdir(parents=True, exist_ok=True)
        info.write_text('"AddonInfo"\n{\n}\n', encoding="utf-8")

    # One -i per file: wildcards match nothing here, and still report success.
    inputs = [argument for path in sources for argument in ("-i", path)]
    flags: list[str | Path] = ["-nop4", "-f", "-game", dirs.client / "game/csgo"]
    result = run(compiler, *flags, *inputs, cwd=compiler.parent, capture=True, check=False)

    # It exits 0 whether or not anything compiled, so the expected outputs decide.
    missing = [path for path in sources if not dirs.compiled_path(path).is_file()]
    if result.returncode != 0 or missing:
        console.info(f"{result.stdout}{result.stderr}".strip())
        if missing:
            names = ", ".join(path.name for path in missing)
            raise VoltmodError(f"resourcecompiler produced no output for: {names}")
        raise VoltmodError(f"resourcecompiler exited {result.returncode}")
    console.note(f"compiled {len(sources)} resource(s)")
