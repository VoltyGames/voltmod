from pathlib import Path

from voltmod.platforms import Platform

INCLUDE_ROOT = Path("include/VoltMod")
SOURCE_DIRS = ("include/VoltMod", "src")

GAMEDATA_FILE = Path("gamedata/gamedata.jsonc")

SCHEMA_DIR = Path("gamedata/schema")
SCHEMA_MANIFEST = SCHEMA_DIR / "manifest.json"
# The Windows and Linux builds of one game version lay classes out differently.
SCHEMA_BASELINES = {platform: SCHEMA_DIR / f"server.{platform}.json" for platform in Platform}
SCHEMA_HEADER_DIR = INCLUDE_ROOT / "Schema"
# Generated headers stay apart from the hand-written ones.
GENERATED_HEADER_DIR = SCHEMA_HEADER_DIR / "Generated"
GENERATED_SOURCE_DIR = Path("src/Schema/Generated")


def is_framework(root: Path) -> bool:
    return (root / INCLUDE_ROOT).is_dir()
