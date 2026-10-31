#!/usr/bin/env python3
"""CTest gate with mandatory preflights and an inventory-backed verdict.

This is orchestration around CTest, not a test scheduler. It never changes memory
admission, stops another process or updates the tested checkout. See
docs/testing/README.md. Verdict classification is shared with the MAP reader.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ctest_verdict import verdict

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git(root, *args):
    p = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    if p.returncode:
        raise RuntimeError("git " + " ".join(args) + ": " + p.stderr.decode(errors="replace"))
    return p.stdout


def identity(root, outputs=()):
    diff = git(root, "diff", "--binary", "HEAD")
    untracked = git(root, "ls-files", "--others", "--exclude-standard", "-z")
    return {"root": str(root), "head": git(root, "rev-parse", "HEAD").decode().strip(),
            "diff_sha256": hashlib.sha256(diff).hexdigest(),
            "status": git(root, "status", "--porcelain=v1").decode(errors="replace"),
            "untracked": {os.fsdecode(p): sha(root / os.fsdecode(p))
                          for p in untracked.split(b"\0") if p and (root / os.fsdecode(p)).is_file()
                          and (root / os.fsdecode(p)).resolve() not in outputs}}


def run(command, log, cwd):
    log.write("COMMAND: " + json.dumps([str(a) for a in command]) + "\n")
    log.flush()
    p = subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    log.flush()
    return p.returncode


def preflight(source, log):
    commands = [
        ["cmake", "-DSOURCE_DIR=" + str(source), "-P", str(ROOT / "cmake/CheckTestNames.cmake")],
        [sys.executable, "-I", str(ROOT / "tools/test/ctest_verdict_test.py")],
    ]
    for command in commands:
        if run(command, log, source):
            raise RuntimeError("mandatory source/verdict preflight failed")


def finish(log, status, ctest_rc=None):
    log.write("GATE_END " + json.dumps({"status": status, "ctest_rc": ctest_rc}) + "\n")
    log.flush()


def ctest_base(args):
    out = ["ctest", "--test-dir", str(args.build_dir)]
    if args.config:
        out += ["-C", args.config]
    return out


def selection(args):
    out = ["-R", args.select]
    for option, value in [("-E", args.exclude), ("-L", args.label), ("-LE", args.exclude_label)]:
        if value:
            out += [option, value]
    return out


def inventory(command, source):
    p = subprocess.run(command + ["--show-only=json-v1"], cwd=source, capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError("CTest inventory failed: " + p.stderr)
    data = json.loads(p.stdout)
    if not isinstance(data.get("tests"), list):
        raise RuntimeError("CTest inventory has no tests array")
    return data


def gate(args):
    args.source_root = args.source_root.resolve()
    source = args.source_root
    if args.preflight_only:
        preflight(source, sys.stdout)
        print("PREFLIGHT: PASS (no project build or test started)")
        return 0
    if args.build_dir is None or args.log is None:
        raise RuntimeError("--build-dir and --log are required for a gate")
    args.build_dir = args.build_dir.resolve()
    args.log = args.log.resolve()
    cache = args.build_dir / "CMakeCache.txt"
    home = next((s.split("=", 1)[1] for s in cache.read_text().splitlines()
                 if s.startswith("CMAKE_HOME_DIRECTORY:INTERNAL=")), None)
    if home is None or Path(home).resolve() != source:
        raise RuntimeError("build directory belongs to a different source tree")
    if args.asset_policy == "unavailable" and os.environ.get("ANNO1503_GAME_DIR"):
        raise RuntimeError("unavailable-assets policy requires ANNO1503_GAME_DIR to be unset")
    lock = Path(str(args.build_dir) + ".run.lock")
    lock.mkdir()  # Same lock as the existing shell runners; never remove someone else's lock.
    try:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        inv = Path(str(args.log) + ".inventory.json")
        outputs = (args.log, inv)
        with args.log.open("x", encoding="utf-8") as log:
            record = {"workflow": identity(ROOT, outputs), "source_before": identity(source, outputs),
                      "started_unix": time.time(), "argv": sys.argv,
                      "asset_policy": args.asset_policy, "allowed_skips": args.allow_skip,
                      "game_dir": os.environ.get("ANNO1503_GAME_DIR"),
                      "configuration": args.config, "jobs": args.jobs,
                      "cmake_cache_sha256": sha(cache), "gate_outputs": [str(p) for p in outputs]}
            log.write("== provenance " + json.dumps(record) + "\n")
            try:
                preflight(source, log)
                if args.target:
                    command = ["cmake", "--build", str(args.build_dir), "--target", args.target]
                    if args.config:
                        command += ["--config", args.config]
                    if run(command, log, source):
                        log.write("VERDICT: FAIL build failed; no selection executed\n")
                        finish(log, 2)
                        return 2
                base = ctest_base(args)
                all_tests = inventory(base, source)["tests"]
                required = list(args.require_test)
                # Evidence contract is compulsory whenever this branch provides it on UNIX.
                if os.name != "nt" and (source / "src/tools/bench_picture/CMakeLists.txt").exists():
                    required.append("picture_matrix_contract")
                names = {t["name"] for t in all_tests}
                for name in dict.fromkeys(required):
                    if name not in names:
                        raise RuntimeError("missing required contract: " + name)
                    # Exact-name lookup followed by a regex escaped for CTest, not Python's
                    # broader escaping rules (spaces need no escape in a CMake regex).
                    escaped = "".join("\\" + c if c in ".[](){}*+?^$|\\" else c for c in name)
                    command = base + ["-R", "^" + escaped + "$", "--no-tests=error",
                                      "--timeout", str(args.timeout), "-V"]
                    log.write("== required contract " + name + "\n")
                    log.flush()
                    start = log.tell()
                    rc = run(command, log, source)
                    with args.log.open(encoding="utf-8") as reader:
                        reader.seek(start)
                        code, rows = verdict(reader.read().splitlines(),
                                             [t for t in all_tests if t["name"] == name], (), True)
                    log.write("\n".join(rows) + "\n")
                    if rc or code:
                        raise RuntimeError("required contract failed: " + name)
                selected = inventory(base + selection(args), source)
                if not selected["tests"]:
                    raise RuntimeError("selection contains no tests")
                with inv.open("x") as f:
                    json.dump(selected, f, indent=2)
                binaries = {}
                for t in selected["tests"]:
                    cmd = t.get("command", [])
                    if cmd and Path(cmd[0]).is_file():
                        binaries.setdefault(cmd[0], None)
                record["binaries"] = {p: sha(p) for p in binaries}
                record["source_tested"] = identity(source, outputs)
                log.write("== tested identity " + json.dumps(record) + "\n")
                log.write("== selection\n")
                rc = run(base + selection(args) + ["--no-tests=error", "-V", "--timeout",
                         str(args.timeout), "-j", str(args.jobs)], log, source)
                allowed = {"platform", "opt-in", *args.allow_skip}
                if args.asset_policy == "unavailable":
                    allowed.add("required-asset")
                code, rows = verdict(args.log.read_text().splitlines(), selected["tests"], allowed, True)
                if identity(source, outputs) != record["source_tested"]:
                    code = 9
                    rows.append("VERDICT: FAIL tested source changed during selection")
                if rc and not code:
                    code = 1
                    rows.append("VERDICT: FAIL CTest returned " + str(rc))
                log.write("\n".join(rows) + "\n")
                finish(log, code, rc)
                print("\n".join(rows))
                print("Evidence: " + str(args.log))
                return code
            except (OSError, ValueError, RuntimeError, KeyError) as e:
                log.write("VERDICT: FAIL workflow: " + str(e) + "\n")
                finish(log, 5)
                raise
    finally:
        lock.rmdir()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--target")
    parser.add_argument("--config", default="")
    parser.add_argument("--select", default=".")
    parser.add_argument("--exclude")
    parser.add_argument("--label")
    parser.add_argument("--exclude-label")
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--timeout", type=int, default=1800)
    parser.add_argument("--asset-policy", choices=["required", "unavailable"], default="required")
    parser.add_argument("--allow-skip", choices=["gpu", "other"], action="append", default=[])
    parser.add_argument("--require-test", action="append", default=[])
    parser.add_argument("--preflight-only", action="store_true")
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout < 1:
        parser.error("jobs and timeout must be positive")
    try:
        return gate(args)
    except (OSError, ValueError, RuntimeError, KeyError) as e:
        print("VERDICT: FAIL workflow: " + str(e), file=sys.stderr)
        return 5


if __name__ == "__main__":
    sys.exit(main())
