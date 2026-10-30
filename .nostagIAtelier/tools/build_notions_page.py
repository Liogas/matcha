#!/usr/bin/env python3
"""Construit la page de lecture du dictionnaire (HTML publié sur claude.ai) et son EPUB.

Usage : python3 .nostagIAtelier/tools/build_notions_page.py
Sortie : .nostagIAtelier/site/notions.html
"""
import base64, html, io, json, pathlib, re, uuid, zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
NOTIONS = ROOT / "notions"
OUT = ROOT / "site"
CATEGORIES = {
    "cpp": "C++", "sql": "PostgreSQL", "auth": "Authentification", "web": "Web et HTTP",
    "temps-reel": "Temps réel", "metier": "Règles de matcha", "archi": "Architecture",
    "tests": "Tests", "build": "Build", "infra": "Infrastructure",
}


def resolve(fiche_id, target):
    parts = []
    for part in (pathlib.PurePosixPath(fiche_id).parent / target).as_posix().split("/"):
        if part == ".." and parts:
            parts.pop()
        elif part != "..":
            parts.append(part)
    return page_id("/".join(parts))


def inline(text, fiche_id):
    text = html.escape(text, quote=False)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\.md\)", lambda m: f'<a class="ref" href="#{resolve(fiche_id, m[2])}">{m[1]}</a>', text)
    text = re.sub(r"`([^`]+)`", r"<code>\1</code>", text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    return text


def page_id(fiche_id):
    return fiche_id.replace("/", "-")


def render(md, fiche_id):
    """Markdown limité des fiches vers HTML : titres, code, listes, citations, liens de fiches."""
    out, code, in_code, lst, in_answer = [], [], False, None, False
    title, summary = "", ""

    def close():
        nonlocal lst
        if lst:
            out.append(f"</{lst}>")
            lst = None

    for line in md.split("\n"):
        if line.startswith("```"):
            if in_code:
                out.append("<pre tabindex=\"0\"><code>" + html.escape("\n".join(code), quote=False) + "</code></pre>")
                code = []
            else:
                close()
            in_code = not in_code
        elif in_code:
            code.append(line)
        elif line.startswith("# "):
            title = line[2:]
        elif line.startswith("### "):
            close()
            out.append("<h3>" + inline(line[4:], fiche_id) + "</h3>")
        elif line.startswith("## "):
            close()
            heading = line.split(" ", 1)[1]
            if in_answer:
                out.append("</details>")
                in_answer = False
            if heading == "Corrigé":
                out.append('<details class="answer"><summary>Voir le corrigé</summary>')
                in_answer = True
                continue
            if heading in ("Pour vérifier que tu as compris", "Exercice"):
                out.append("<h2 class=\"check\">" + inline(heading, fiche_id) + "</h2>")
            else:
                out.append("<h2>" + inline(heading, fiche_id) + "</h2>")
        elif line.startswith("> "):
            summary = re.sub(r"\[([^\]]+)\]\([^)]+\.md\)", r"\1", line[2:]).replace("`", "")
        elif (m := re.fullmatch(r"- \[(.+)\]\((.+)\.md\)", line)):
            if lst != "ul":
                close()
                out.append('<ul class="see">')
                lst = "ul"
            out.append(f'<li><a class="ref" href="#{resolve(fiche_id, m[2])}">{inline(m[1], fiche_id)}</a></li>')
        elif line.startswith("- "):
            if lst != "ul":
                close()
                out.append("<ul>")
                lst = "ul"
            out.append("<li>" + inline(line[2:], fiche_id) + "</li>")
        elif (m := re.match(r"\d+\. (.*)", line)):
            if lst != "ol":
                close()
                out.append("<ol>")
                lst = "ol"
            out.append("<li>" + inline(m[1], fiche_id) + "</li>")
        elif not line.strip():
            close()
        else:
            close()
            out.append("<p>" + inline(line, fiche_id) + "</p>")
    close()
    if in_answer:
        out.append("</details>")
    return title, summary, "\n".join(out)


def load():
    board = json.loads((ROOT / "board.json").read_text(encoding="utf8"))
    cards = []
    linked = {}
    for column in board["columns"]:
        for card in column["cards"]:
            cards.append({"id": card["id"], "name": card["name"], "column": column["id"],
                          "priority": card["priority"], "notions": [page_id(n) for n in card["notions"]]})
            for notion in card["notions"]:
                linked.setdefault(page_id(notion), []).append(card["id"])
    fiches = []
    for category in CATEGORIES:
        for path in sorted((NOTIONS / category).glob("*.md")):
            fiche_id = f"{category}/{path.stem}"
            md = path.read_text(encoding="utf8")
            title, summary, body = render(md, fiche_id)
            fiches.append({"id": page_id(fiche_id), "cat": category, "title": title, "summary": summary,
                           "body": body, "md": md, "cards": linked.get(page_id(fiche_id), [])})
    # La lecture commence par les bases, pas par l'ordre alphabétique
    fiches.sort(key=lambda f: (list(CATEGORIES).index(f["cat"]), f["title"].lower()))
    return fiches, cards


def epub(fiches):
    book_id = uuid.uuid5(uuid.NAMESPACE_URL, "matcha-notions")
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as z:
        z.writestr("mimetype", "application/epub+zip", zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml", '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        manifest, spine, nav = [], [], []
        current = None
        for fiche in fiches:
            name = fiche["id"] + ".xhtml"
            body = re.sub(r'href="#([a-z0-9-]+)"', r'href="\1.xhtml"', fiche["body"]).replace(' tabindex="0"', "")
            z.writestr("OEBPS/" + name, f'<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" xml:lang="fr"><head><title>{html.escape(fiche["title"])}</title><link rel="stylesheet" href="s.css"/></head><body><h1>{html.escape(fiche["title"])}</h1><p class="s">{html.escape(fiche["summary"])}</p>{body}</body></html>')
            manifest.append(f'<item id="{fiche["id"]}" href="{name}" media-type="application/xhtml+xml"/>')
            spine.append(f'<itemref idref="{fiche["id"]}"/>')
            if fiche["cat"] != current:
                if current:
                    nav.append("</ol></li>")
                current = fiche["cat"]
                nav.append(f'<li><a href="{name}">{html.escape(CATEGORIES[current])}</a><ol>')
            nav.append(f'<li><a href="{name}">{html.escape(fiche["title"])}</a></li>')
        nav.append("</ol></li>")
        z.writestr("OEBPS/s.css", "body{line-height:1.5}pre{white-space:pre-wrap;font-size:.8em;background:#eef1e6;padding:.6em}code{font-family:monospace}.s{font-style:italic}")
        z.writestr("OEBPS/nav.xhtml", '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="fr"><head><title>Sommaire</title></head><body><nav epub:type="toc"><h1>Sommaire</h1><ol>' + "".join(nav) + "</ol></nav></body></html>")
        z.writestr("OEBPS/content.opf", f'<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:uuid:{book_id}</dc:identifier><dc:title>Dictionnaire matcha</dc:title><dc:language>fr</dc:language><meta property="dcterms:modified">2026-10-30T00:00:00Z</meta></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="css" href="s.css" media-type="text/css"/>{"".join(manifest)}</manifest><spine>{"".join(spine)}</spine></package>')
    return buffer.getvalue()


def question_id(notion, text):
    """Même empreinte que l'outil (QuizBank) : les résultats du téléphone restent comparables."""
    value = 14695981039346656037
    for byte in (notion + text).encode("utf8"):
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return str(value)


def load_quiz():
    questions = []
    for path in sorted((ROOT / "quiz").glob("*/*.json")):
        notion = path.relative_to(ROOT / "quiz").with_suffix("").as_posix()
        for q in json.loads(path.read_text(encoding="utf8")).get("questions", []):
            questions.append({"id": question_id(notion, q["text"]), "n": page_id(notion), "t": inline(q["text"], notion),
                              "c": [inline(c, notion) for c in q["choices"]], "k": q["correct"],
                              "e": inline(q.get("explanation", ""), notion)})
    return questions


def load_answers():
    """Dernière réponse à chaque question dans l'outil : les points faibles se retrouvent sur le téléphone."""
    answers = {}
    path = ROOT / "quiz" / "results.jsonl"
    if path.exists():
        for line in path.read_text(encoding="utf8").splitlines():
            answers.update(json.loads(line).get("answers", {}))
    return answers


def main():
    fiches, cards = load()
    OUT.mkdir(exist_ok=True)
    book = epub(fiches)
    (OUT / "dictionnaire-matcha.epub").write_bytes(book)
    template = (pathlib.Path(__file__).parent / "notions_page.html").read_text(encoding="utf8")
    data = json.dumps({"fiches": fiches, "cards": cards, "categories": CATEGORIES,
                       "quiz": load_quiz(), "answers": load_answers(),
                       "epub": base64.b64encode(book).decode()}, ensure_ascii=False)
    data = data.replace("</", "<\\/")
    (OUT / "notions.html").write_text(template.replace("/*DATA*/null", data), encoding="utf8")
    print(len(fiches), "fiches,", (OUT / "notions.html").stat().st_size // 1024, "Ko")


main()
