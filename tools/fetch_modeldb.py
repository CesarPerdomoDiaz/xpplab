"""Download the XPPAUT models published on ModelDB (modeldb.science).

ModelDB lists the simulator of every model. Those listed under XPPAUT (or
"XPP (web link to model)") are downloaded as the zip ModelDB offers and
unpacked into one folder per model, build-modeldb/models/<id>/. The models
carry their own licences, so they are not committed: tests/modeldb/manifest.json
records what was fetched (id, title, the zip's SHA-256, and every model file
with the tables and include files it needs, each with its URL and SHA-256)
and later runs check the downloads against it.

    python -I tools/fetch_modeldb.py             # the models in the manifest
    python -I tools/fetch_modeldb.py --refresh   # ask ModelDB again, rewrite it
    python -I tools/fetch_modeldb.py --only 45513

The downloads are untrusted data. Nothing in them is executed. Unpacking
rejects absolute paths, '..', links and names Windows cannot hold, caps the
size of each model, and leaves out programs, scripts and libraries: XPPAUT
calls system("clear") when it loads a model and dlopen()s libraries named in
it, and on Windows both look in the current directory first. Run this with
-I so that no file in the current directory can shadow a module.
"""
import argparse
import hashlib
import io
import json
import re
import shutil
import sys
import time
import urllib.parse
import urllib.request
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / "build-modeldb" / "models"
MANIFEST = ROOT / "tests" / "modeldb" / "manifest.json"
SITE = "https://modeldb.science"
API = SITE + "/api/v1/models"
SIMULATORS = {"XPPAUT", "XPP (web link to model)"}
USER_AGENT = "xppstudio-modeldb-check/1 (+https://github.com/CesarPerdomoDiaz/xppstudio)"

MAX_FILE = 100 << 20   # larger members (recordings, movies) are skipped
MAX_MODEL = 300 << 20  # and a model stops unpacking past this total
WINDOWS_BAD = re.compile(r'[<>:"|?*\x00-\x1f\\]')
WINDOWS_RESERVED = re.compile(r"^(con|prn|aux|nul|com\d|lpt\d)(\..*)?$", re.I)
JUNK = re.compile(r"(^|/)(__MACOSX/|\.DS_Store$|\._)")
# Anything Windows' cmd.exe or a loader would run, and native code.
RUNNABLE = {
    ".exe", ".com", ".bat", ".cmd", ".ps1", ".psm1", ".vbs", ".vbe", ".js",
    ".jse", ".wsf", ".wsh", ".msc", ".msi", ".scr", ".pif", ".cpl", ".lnk",
    ".hta", ".reg", ".dll", ".so", ".dylib", ".o", ".obj", ".a", ".lib",
    ".jar", ".class", ".pyc", ".pyd", ".mexw64", ".mexa64", ".mexmaci64",
}
BINARY_MAGIC = (b"MZ", b"\x7fELF", b"\xfe\xed\xfa", b"\xce\xfa\xed\xfe",
                b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe")
# XPPAUT does not care about the extension: some models are .ode.txt,
# .xpp or .txt files.
MODEL_SUFFIXES = (".ode", ".xpp", ".txt")
EQUATION = re.compile(r"^\s*(\w+'|d\w+/dt|\w+\(t\+1\))\s*=", re.I | re.M)
DECLARATION = re.compile(r"^\s*(done\b|@\s|(p|par|param|init|aux)\s+\w+\s*=)",
                         re.I | re.M)
TABLE = re.compile(r"^\s*tab(le)?\s+\w+\s+([^\s%][^\s]*)", re.I | re.M)
INCLUDE = re.compile(r"^\s*#include\s+<?([^\s>]+)", re.I | re.M)


def get(url, tries=4):
    for attempt in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=120) as resp:
                return resp.read()
        except OSError:
            if attempt == tries - 1:
                raise
            time.sleep(2 + 4 * attempt)


def get_json(url):
    return json.loads(get(url).decode("utf-8"))


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def file_url(model_id, name):
    return (f"{SITE}/getModelFile?model={model_id}&file="
            + urllib.parse.quote(name, safe="/"))


def list_models():
    """Ids, titles and simulators of every ModelDB model for XPPAUT."""
    ids = get_json(API)
    names = get_json(API + "/name")
    apps = get_json(API + "/modeling_application")
    dates = get_json(API + "/ver_date")
    found = []
    for mid, name, app, date in zip(ids, names, apps, dates):
        sims = [v["object_name"] for v in (app or {}).get("value", [])]
        if SIMULATORS & set(sims):
            found.append({"id": mid, "title": name, "simulators": sims,
                          "ver_date": date})
    return sorted(found, key=lambda m: m["id"])


def safe_name(name):
    """The member's path inside the model folder, or None and a reason."""
    if name.endswith("/"):
        return None, "directory"
    if JUNK.search(name):
        return None, "macOS metadata"
    p = PurePosixPath(name)
    if p.is_absolute() or re.match(r"^[A-Za-z]:", name):
        return None, "absolute path"
    parts = p.parts
    if not parts or any(s in ("", ".", "..") for s in parts):
        return None, "relative path outside the model"
    for s in parts:
        if (WINDOWS_BAD.search(s) or WINDOWS_RESERVED.match(s)
                or s.endswith((" ", "."))):
            return None, "name not valid on Windows"
    if p.suffix.lower() in RUNNABLE:
        return None, "program or library"
    return p, None


def is_binary_program(body):
    return body.startswith(BINARY_MAGIC)


class Unpacker:
    """Unpack one model's zip, and zips inside it, into its folder."""

    def __init__(self, model_id, dest):
        self.model_id, self.dest = model_id, dest
        self.files, self.skipped, self.seen, self.total = {}, [], set(), 0

    def skip(self, name, reason, **extra):
        self.skipped.append(dict(name=name, reason=reason, **extra))

    def run(self, data, prefix=PurePosixPath(), source=None):
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for info in z.infolist():
                shown = str(prefix / info.filename) if source else info.filename
                rel, why = safe_name(info.filename)
                if why == "directory":
                    # Keep empty folders: some models write output into one.
                    folder, bad = safe_name(info.filename.rstrip("/") + "/x")
                    if folder is not None:
                        self.dest.joinpath(*(prefix / folder.parent).parts) \
                            .mkdir(parents=True, exist_ok=True)
                    continue
                if rel is None:
                    if why != "macOS metadata":
                        self.skip(shown, why)
                    continue
                if (info.external_attr >> 16) & 0o170000 == 0o120000:
                    self.skip(shown, "symbolic link")
                    continue
                if (info.file_size > MAX_FILE
                        or self.total + info.file_size > MAX_MODEL):
                    self.skip(shown, "too large", size=info.file_size)
                    continue
                body = z.read(info)
                if is_binary_program(body):
                    self.skip(shown, "program or library")
                    continue
                rel = prefix / rel
                if rel.suffix.lower() == ".zip" and source is None:
                    # One level of nesting: unpack next to the zip.
                    self.run(body, rel.with_suffix(""), str(rel))
                    continue
                key = str(rel).lower()  # Windows and macOS ignore case
                if key in self.seen:
                    self.skip(shown, "duplicate name")
                    continue
                self.seen.add(key)
                self.total += len(body)
                target = self.dest.joinpath(*rel.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(body)
                entry = {"name": str(rel), "size": len(body),
                         "sha256": sha256(body)}
                if source:
                    entry["in"] = source
                    entry["url"] = file_url(self.model_id, source)
                else:
                    entry["url"] = file_url(self.model_id, info.filename)
                self.files[key] = entry


def looks_like_model(name, text):
    if name.lower().endswith(".ode"):
        return True
    return bool(EQUATION.search(text) and DECLARATION.search(text))


def needed_files(text):
    """Files a model reads when it is loaded: tables and includes."""
    return [m.group(2) for m in TABLE.finditer(text)] + \
           [m.group(1) for m in INCLUDE.finditer(text)]


def describe(files, dest):
    """Pick out the model files and the files they need.

    XPPAUT opens tables and includes relative to the current directory. A
    model normally runs in its own folder; when a file it needs is only in
    a folder above (one table shared by several models), it runs there."""
    models, needs, missing = [], set(), []
    for key, f in sorted(files.items()):
        if not key.endswith(MODEL_SUFFIXES) or f["size"] > (2 << 20):
            continue
        text = dest.joinpath(*PurePosixPath(f["name"]).parts) \
                   .read_text(encoding="latin-1")
        if not looks_like_model(f["name"], text):
            continue
        own = PurePosixPath(f["name"]).parent
        run_in = own
        for ref in needed_files(text):
            ref = ref.replace("\\", "/")
            for folder in [run_in] + [p for p in own.parents if p != run_in]:
                k = str(folder / ref).lower()
                if k in files:
                    needs.add(k)
                    run_in = folder
                    break
            else:
                missing.append({"model": f["name"], "file": ref})
        models.append({"name": f["name"], "run_in": str(run_in)})
    keys = {m["name"].lower() for m in models} | needs
    return models, [files[k] for k in sorted(keys)], missing


def fetch(entry, cache):
    mid = entry["id"]
    url = f"{SITE}/download/{mid}"
    data = get(url)
    dest = cache / str(mid)
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    u = Unpacker(mid, dest)
    u.run(data)
    models, listed, missing = describe(u.files, dest)
    out = dict(entry, url=f"{SITE}/{mid}", download=url,
               zip_sha256=sha256(data), models=models, files=listed,
               other_files=len(u.files) - len(listed))
    if missing:
        out["missing"] = missing
    if u.skipped:
        out["skipped"] = u.skipped
    return out


def check(old, new):
    """Differences between the manifest entry and what was downloaded."""
    if old.get("zip_sha256") == new["zip_sha256"]:
        return []
    problems = []
    was = {f["name"]: f["sha256"] for f in old.get("files", [])}
    now = {f["name"]: f["sha256"] for f in new["files"]}
    for name in sorted(set(was) | set(now)):
        if name not in now:
            problems.append(f"{name}: missing")
        elif name not in was:
            problems.append(f"{name}: new file")
        elif was[name] != now[name]:
            problems.append(f"{name}: SHA-256 changed")
    return problems or ["zip changed, model files the same"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--refresh", action="store_true",
                    help="list the models on ModelDB again and rewrite the manifest")
    ap.add_argument("--only", type=int, action="append",
                    help="fetch only this model id (repeatable)")
    ap.add_argument("--cache", type=Path, default=CACHE)
    ap.add_argument("--manifest", type=Path, default=MANIFEST)
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()

    old = {}
    if args.manifest.exists():
        old = {m["id"]: m for m in
               json.loads(args.manifest.read_text(encoding="utf-8"))["models"]}
    if args.refresh or not old:
        listed = list_models()
        print(f"ModelDB lists {len(listed)} XPPAUT models")
    else:
        keys = ("id", "title", "simulators", "ver_date")
        listed = [{k: m[k] for k in keys} for m in old.values()]
    if args.only:
        listed = [m for m in listed if m["id"] in args.only]

    args.cache.mkdir(parents=True, exist_ok=True)
    changed, results = 0, {}
    with ThreadPoolExecutor(args.jobs) as pool:
        futures = [(m, pool.submit(fetch, m, args.cache)) for m in listed]
        for m, fut in futures:
            try:
                new = fut.result()
            except Exception as e:  # keep going; report at the end
                print(f"FAILED  {m['id']}: {e}")
                continue
            results[m["id"]] = new
            problems = check(old[m["id"]], new) if m["id"] in old else []
            changed += bool(problems)
            print(f"{'CHANGED' if problems else 'ok':7s} {m['id']:>7} "
                  f"{len(new['models'])} model(s), {len(new['files'])} "
                  f"listed, {new['other_files']} other  {'; '.join(problems)}")
    failed = len(listed) - len(results)

    if args.refresh or not old:
        models = dict(old)
        if not args.only:  # drop models ModelDB no longer lists
            models = {i: m for i, m in models.items()
                      if i in {e["id"] for e in listed}}
        models.update(results)
        manifest = {
            "source": SITE,
            "fetched": time.strftime("%Y-%m-%d"),
            "note": "Model files are not in this repository; each model "
                    "carries its own licence. Fetch them with "
                    "tools/fetch_modeldb.py.",
            "models": [models[i] for i in sorted(models)],
        }
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps(manifest, indent=1,
                                            ensure_ascii=False) + "\n",
                                 encoding="utf-8")
        print(f"wrote {args.manifest}")
    print(f"{len(results)} fetched into {args.cache}, {changed} changed, "
          f"{failed} failed")
    return 1 if failed or (changed and not args.refresh) else 0


if __name__ == "__main__":
    sys.exit(main())
