#!/usr/bin/env python3
"""Source-derived, default-fail catalogue audit for active TUI renderers."""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
# WHY: implementation-directory membership is not presentation reachability;
# scanning core SQL, synchronization codecs, and catalogs confuses durable raw
# diagnostics/identifiers with renderer copy.
# CONTRACT: this reviewed allowlist contains every active renderer plus the two
# known presentation-producing helpers. A new renderer must be added here and
# defaults to failure as soon as it contains uncatalogued French copy.
# INVARIANT: database/sync/sync_history/catalog implementations are not UI-copy
# owners and cannot become implicit exceptions through this audit.
RENDERER_SOURCES = (
    "tui/src/tui.c",
    "tui/src/chart.c",
    "tui/src/app_shell.c",
    "tui/src/sync_screen_action.c",
)
PRESENTATION_PRODUCERS = (
    "tui/src/timeutil.c",
    "tui/src/statistics.c",
)
SOURCES = RENDERER_SOURCES + PRESENTATION_PRODUCERS
assert not {"tui/src/database.c", "tui/src/sync.c", "tui/src/sync_history.c",
            "tui/src/catalog.c"}.intersection(SOURCES)
RENDERER_MARKER = re.compile(
    r"trainlog_(?:surface|terminal)_(?:printf|put|draw|set)|"
    r"trainlog_presentation_(?:text|source|format)"
)
INFRASTRUCTURE = {"tui/src/presentation.c", "tui/src/terminal.c"}
discovered_renderers = {
    str(path.relative_to(ROOT))
    for path in (ROOT / "tui/src").glob("*.c")
    if RENDERER_MARKER.search(path.read_text(encoding="utf-8"))
} - INFRASTRUCTURE
unreviewed_renderers = discovered_renderers - set(RENDERER_SOURCES)
if unreviewed_renderers:
    sys.stderr.write("UNREVIEWED presentation renderer(s):\n" +
                     "\n".join(f"  {path}" for path in sorted(unreviewed_renderers)) + "\n")
    sys.exit(1)
P = (ROOT / "tui/src/presentation.c").read_text(encoding="utf-8")
CATALOG = set(re.findall(r'\{\s*"(?:[^"\\]|\\.)*"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"', P))
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
FR = re.compile(r'[À-ÿ]|\b(?:aucun|aucune|abandonner|annuler|choisir|durée|entrée|exercice|fusion|historique|impossible|mesure|objectif|profil|recherche|séance|synchron|taille|zone|série|charge)\b', re.I)

# Exact source-bound exceptions, never pattern exemptions.  Each is a
# non-copy token: punctuation/layout, a protocol token, or an internal key.
EXCLUDED = {
 "tui/src/tui.c": {
  1083:"—",1495:"— kg",3970:"J−%zu",3973:"M−%zu",3976:"P−%zu",4759:"msg.zone.required",5204:"statistics.zone.previous",5206:"statistics.zone.next",5270:"generator.warning.zone",5612:"—",5655:"— ",5660:"— ",5678:"— ",5693:"—",5696:"—",5820:"—",5823:"—",5826:"—",5881:"—",6312:"%-22s —",6612:"▶",6615:"▶",6643:"▶",6652:"▶",6665:"▶",6668:"▶",6692:"▶",6697:"▶",6888:"%02d/%02d … %02d/%02d",6930:"— ",7040:"PC↔Android",7086:"▶",7310:"%s · %s",7318:"—",7319:"—",7395:"—",7398:"—",7567:"—",7595:"—",7597:"—",7599:"—",7717:"aucun",7719:"aucun",7884:"—",7935:"—",8065:" · ↑",8066:" · ↓"},
 "tui/src/chart.c": {254:"◆ %.2f %s"},
 "tui/src/app_shell.c": {317:"…"},
 "tui/src/sync_screen_action.c": {132:"Android→PC",136:"PC→Android",139:"PC↔Android"},
 "tui/src/statistics.c": {230:("août", "déc.")},
}

def candidates(text, path):
    """Independent source scanner: adding uncatalogued French copy fails."""
    out=[]
    for line_no,line in enumerate(text.splitlines(),1):
        for match in LIT.finditer(line):
            value=match.group(1)
            if FR.search(value): out.append((path,line_no,value))
    return out

# Non-tautology proof: this never appears in the catalogue/manifest.
probe=candidates('trainlog_surface_printf(s, 1, 1, "Bonjour séance");','fixture.c')
assert probe == [('fixture.c',1,'Bonjour séance')]
assert probe[0][2] not in CATALOG
assert RENDERER_MARKER.search('trainlog_surface_printf(s, 1, 1, "new copy")')

unknown=[]; used=set()
for path in SOURCES:
    for candidate in candidates((ROOT/path).read_text(encoding='utf-8'),path):
        _,line,value=candidate
        if value in CATALOG: continue
        allowed = EXCLUDED.get(path,{}).get(line)
        if allowed == value or isinstance(allowed, tuple) and value in allowed:
            used.add(candidate); continue
        unknown.append(candidate)
if unknown:
    for kind, rows in (("TRANSLATABLE_UI catalogue omissions",unknown),):
        if rows: sys.stderr.write(kind+':\n'+'\n'.join('  %s:%s: %s'%r for r in rows)+'\n')
    sys.exit(1)
print('TRANSLATABLE_UI=0 unexplained; source-derived exclusions:',len(used))
