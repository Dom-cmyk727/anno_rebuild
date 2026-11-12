"""Public CLI contract for the independent freight reference (no game/assets)."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    prog = root / "tools/freight_ref.py"
    fixture = root / "fixtures/synthetic/sim/freight/contract_vectors.jsonl"
    cases = [json.loads(line) for line in fixture.read_text().splitlines()]
    probes = 0

    def invoke(args, passed, diagnostic):
        nonlocal probes
        proc = subprocess.run([sys.executable, "-I", str(prog)] + args, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        out = proc.stdout + proc.stderr
        if (proc.returncode == 0) != passed or diagnostic not in out or "Traceback" in out:
            raise AssertionError((args, proc.returncode, out))
        probes += 1

    invoke(["check", str(fixture)], True, f"executed {len(cases)} vectors")
    invoke(["selftest", str(fixture)], True, "selftest passed")
    with tempfile.TemporaryDirectory(prefix="freight-reference-contract-") as tmp:
        path = Path(tmp) / "vectors.jsonl"

        def probe(value, diagnostic, raw=False):
            path.write_text(value if raw else json.dumps(value) + "\n")
            invoke(["check", str(path)], False, diagnostic)

        probe("\n \n", "zero executed vectors", True)
        probe('{"name":', "freight reference:", True)
        probe('{"name":"a","name":"b"}', "duplicate JSON key", True)
        probe([], "malformed names")
        probe({}, "wrong object fields")
        wrong = copy.deepcopy(cases[0]); wrong["expect"] = {"wrong": True}
        probe(wrong, "mismatch")
        changed = copy.deepcopy(cases[0]); changed["input"]["actions"][0]["tons"] = 3
        probe(changed, "mismatch")  # changed legal input must be computed, not echoed
        for bad in [-1, True, 1.5, "2", 72057594037927936]:
            wrong = copy.deepcopy(cases[0]); wrong["input"]["actions"][0]["tons"] = bad
            probe(wrong, "integer outside declared domain")
        wrong = copy.deepcopy(cases[0]); wrong["input"]["actions"][0]["direction"] = 2
        probe(wrong, "integer outside declared domain")
        wrong = copy.deepcopy(cases[0]); wrong["input"]["holds"] = [[42]]
        probe(wrong, "hold shape")
        wrong = copy.deepcopy(cases[0]); wrong["input"]["stock"] = {"042": 256}
        probe(wrong, "stock goods key")
        wrong = copy.deepcopy(cases[0]); wrong["input"]["actions"] = [
            {"direction": 0, "goods": 42, "tons": 72057594037927935},
            {"direction": 0, "goods": 42, "tons": 1}]
        probe(wrong, "group quantity overflow")
        probe(json.dumps(cases[0]) + "\n" + json.dumps(cases[0]), "duplicate/malformed names", True)
        wrong = copy.deepcopy(next(case for case in cases if case["kind"] == "assign_wire"))
        wrong["input"]["stops"] = []
        probe(wrong, "nonempty stop list")
        invoke(["check", str(Path(tmp) / "missing.jsonl")], False, "freight reference:")
        invoke(["unknown", str(fixture)], False, "usage:")
        invoke([], False, "usage:")
    print(f"freight reference CLI contract: {probes} probes passed")


if __name__ == "__main__":
    main()
