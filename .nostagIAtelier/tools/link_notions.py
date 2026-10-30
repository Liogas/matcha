#!/usr/bin/env python3
"""Relie les fiches entre elles : chaque terme de aliases.json devient un lien vers sa fiche.

Le premier emploi d'un terme dans chaque section est lié, jamais dans le code ni les titres.
Idempotent : les liens en ligne sont retirés puis recalculés.
Usage : python3 .nostagIAtelier/tools/link_notions.py
"""
import json, os, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[1]
NOTIONS = ROOT / "notions"
ALIASES = json.loads((pathlib.Path(__file__).parent / "aliases.json").read_text(encoding="utf8"))

term_to_id = {}
for fiche_id, terms in ALIASES.items():
    assert (NOTIONS / f"{fiche_id}.md").exists(), fiche_id
    for term in terms:
        assert term.lower() not in term_to_id, term
        term_to_id[term.lower()] = fiche_id
sigles = {t.lower() for terms in ALIASES.values() for t in terms if t.isupper()}
pattern = re.compile(r"(?<![\w-])(" + "|".join(re.escape(t) for t in sorted(term_to_id, key=len, reverse=True)) + r")(?![\w-])",
                     re.IGNORECASE)
INLINE_LINK = re.compile(r"\[([^\]]+)\]\([^)]*\.md\)")


def link_to(target, here):
    return os.path.relpath(f"{target}.md", os.path.dirname(here))


def process(text, here, seen):
    def plain(segment):
        def replace(match):
            target = term_to_id[match[1].lower()]
            # Un sigle ne se reconnaît qu'en majuscules : « ci » n'est pas CI
            shouted = match[1].lower() in sigles and not match[1].isupper()
            if target == here or target in seen or shouted:
                return match[0]
            seen.add(target)
            return f"[{match[0]}]({link_to(target, here)})"
        return pattern.sub(replace, segment)

    parts = re.split(r"(`[^`]+`)", text)
    for i, part in enumerate(parts):
        if i % 2 == 0:
            parts[i] = plain(part)
        else:
            target = term_to_id.get(part[1:-1].lower())
            if target and target != here and target not in seen:
                seen.add(target)
                parts[i] = f"[{part}]({link_to(target, here)})"
    return "".join(parts)


total = 0
for path in sorted(NOTIONS.glob("*/*.md")):
    here = path.relative_to(NOTIONS).with_suffix("").as_posix()
    out, seen, in_code, in_see_also = [], set(), False, False
    for line in path.read_text(encoding="utf8").split("\n"):
        if line.startswith("```"):
            in_code = not in_code
        elif not in_code and line.startswith("#"):
            seen = set()
            in_see_also = line.startswith("## Voir aussi")
        elif not in_code and not in_see_also and line.strip():
            line = INLINE_LINK.sub(r"\1", line)
            line = process(line, here, seen)
        out.append(line)
    text = "\n".join(out)
    total += text.count("](") 
    path.write_text(text, encoding="utf8")
print(total, "liens")
