import re
from pathlib import Path

from voltmod.errors import VoltmodError

# Mirrors DialectFor in include/VoltMod/Database/Migrator.hpp; test_database.py fails on drift.
# @ON_CONFLICT(columns)@ is resolved in code: only Postgres renders a clause for it.
DIALECTS: dict[str, dict[str, str]] = {
    "postgres": {
        "ID": "BIGSERIAL PRIMARY KEY",
        "NOW": "EXTRACT(EPOCH FROM NOW())::BIGINT",
        "TRUE": "TRUE",
        "FALSE": "FALSE",
        "INSERT_IF_ABSENT": "INSERT INTO",
    },
    "mariadb": {
        "ID": "BIGINT AUTO_INCREMENT PRIMARY KEY",
        "NOW": "(UNIX_TIMESTAMP())",
        "TRUE": "TRUE",
        "FALSE": "FALSE",
        "INSERT_IF_ABSENT": "INSERT IGNORE INTO",
    },
    "sqlite": {
        # 1/0 rather than TRUE/FALSE: it is what an existing database's stored schema text says.
        "ID": "INTEGER PRIMARY KEY AUTOINCREMENT",
        "NOW": "(strftime('%s','now'))",
        "TRUE": "1",
        "FALSE": "0",
        "INSERT_IF_ABSENT": "INSERT OR IGNORE INTO",
    },
}

DRIVERS = tuple(DIALECTS)

_PLACEHOLDER = re.compile(r"@([A-Z_]+(?:\([^)@]*\))?)@")
_CONFLICT_PREFIX = "ON_CONFLICT("
_SCHEMA_CHANGE = re.compile(
    r"^\s*(?:ALTER\s+TABLE\s+(?P<table>\w+)\s+(?P<action>ADD|DROP)\s+COLUMN\s+"
    r"(?:IF\s+(?:NOT\s+)?EXISTS\s+)?(?P<column>\w+)(?P<definition>[^;]*)"
    r"|DROP\s+TABLE\s+(?:IF\s+EXISTS\s+)?(?P<dropped>\w+)\s*);[^\S\n]*\n?",
    re.I | re.M,
)


def resolve_placeholders(sql: str, driver: str) -> str:
    """Substitute every @PLACEHOLDER@ in `sql` for `driver`; an unknown one is an error."""
    if driver not in DIALECTS:
        raise VoltmodError(f"unknown driver '{driver}'; expected {', '.join(DRIVERS)}")
    values = DIALECTS[driver]

    def replace(match: re.Match[str]) -> str:
        name = match.group(1)
        if name.startswith(_CONFLICT_PREFIX):
            columns = name.removeprefix(_CONFLICT_PREFIX).removesuffix(")")
            return f"ON CONFLICT ({columns}) DO NOTHING" if driver == "postgres" else ""
        if name not in values:
            raise VoltmodError(f"unknown migration placeholder @{name}@")
        return values[name]

    return _PLACEHOLDER.sub(replace, sql)


def find_migrations(directory: Path) -> list[Path]:
    """Every `NNNN_*.sql` in `directory`, in version order."""
    numbered = []
    for path in directory.glob("*.sql"):
        if match := re.match(r"(\d+)", path.name):
            numbered.append((int(match.group(1)), path))
    if not numbered:
        raise VoltmodError(f"no NNNN_*.sql migrations in {directory}")
    return [path for _, path in sorted(numbered)]


def render_migrations(source: Path, driver: str) -> str:
    """One SQL file, or a whole migration directory in version order, rendered for `driver`."""
    files = [source] if source.is_file() else find_migrations(source)
    rendered = (resolve_placeholders(path.read_text(encoding="utf-8"), driver) for path in files)
    return "\n".join(rendered)


def apply_schema_changes(ddl: str) -> str:
    """Fold each `ALTER TABLE t ADD|DROP COLUMN` and `DROP TABLE t` into the CREATE TABLE before it.

    ddl2cpp only reads CREATE TABLE. Going in file order lets a migration drop a table and
    create it again under the same name.
    """
    while change := _SCHEMA_CHANGE.search(ddl):
        table = change["table"] or change["dropped"]
        before = ddl[: change.start()]
        creates = list(
            re.finditer(
                rf"CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?{table}\s*\((.*?)\n\);[^\S\n]*\n?",
                before,
                re.I | re.S,
            )
        )
        if not creates:
            raise VoltmodError(
                f"{change[0].strip()} names a table with no CREATE TABLE before it: {table}"
            )
        create = creates[-1]
        if change["dropped"]:
            before = before[: create.start()] + before[create.end() :]
        else:
            body = _alter_column(
                create[1], table, change["action"], change["column"], change["definition"]
            )
            before = before[: create.start(1)] + body + before[create.end(1) :]
        ddl = before + ddl[change.end() :]
    return ddl


def _alter_column(body: str, table: str, action: str, column: str, definition: str) -> str:
    if action.upper() == "ADD":
        return f"{body.rstrip()},\n  {column}{definition.rstrip()}"
    dropped = re.sub(rf"^\s*{column}\s[^\n]*\n?", "", body, count=1, flags=re.I | re.M)
    if dropped == body:
        raise VoltmodError(f"DROP COLUMN names a column {table} does not have: {column}")
    # The dropped column may have been the last one.
    return dropped.rstrip().removesuffix(",")
