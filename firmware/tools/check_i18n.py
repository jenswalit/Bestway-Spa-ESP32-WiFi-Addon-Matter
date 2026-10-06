#!/usr/bin/env python3

import json
from pathlib import Path

LANG_DIR = Path(
    "components/web_server/web/lang"
)

CATALOG = LANG_DIR / "languages.json"
BASE = LANG_DIR / "nl.json"

def load(path):
    with path.open(
        "r",
        encoding="utf-8"
    ) as f:
        return json.load(f)

languages = load(CATALOG)
base = load(BASE)

base_keys = {
    k for k in base.keys()
    if k != "_phrases"
}

base_phrases = set(
    base.get("_phrases", {}).keys()
)

print()
print("=== Spa Control vertaalcontrole ===")
print()

errors = 0

for code, name in languages.items():

    path = LANG_DIR / f"{code}.json"

    if not path.exists():
        print(
            f"❌ {code:3} {name}: bestand ontbreekt"
        )
        errors += 1
        continue

    try:
        data = load(path)
    except Exception as e:
        print(
            f"❌ {code:3} {name}: JSON fout: {e}"
        )
        errors += 1
        continue

    keys = {
        k for k in data.keys()
        if k != "_phrases"
    }

    phrases = set(
        data.get("_phrases", {}).keys()
    )

    missing = sorted(base_keys - keys)
    extra = sorted(keys - base_keys)

    missing_phrases = sorted(
        base_phrases - phrases
    )

    if (
        not missing
        and not missing_phrases
    ):
        print(
            f"✅ {code:3} {name}: compleet"
        )
    else:
        print(
            f"⚠️  {code:3} {name}: "
            f"{len(missing)} keys en "
            f"{len(missing_phrases)} phrases ontbreken"
        )

        for key in missing[:20]:
            print(f"     - {key}")

        if len(missing) > 20:
            print(
                f"     ... plus "
                f"{len(missing)-20}"
            )

        if missing_phrases:
            print(
                f"     phrases ontbreken: "
                f"{len(missing_phrases)}"
            )

    if extra:
        print(
            f"     ℹ {len(extra)} extra keys"
        )

print()
