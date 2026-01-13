#!/usr/bin/env python3
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

TEST_DIR = Path("examples/test")
INTERPRETER = Path("c_using_llvm/interpreter")
LLVM_COMPILER = Path("c_using_llvm/codegen_llvm")


def read_expectations(path: Path):
    """
    Collect expected output snippets from:
    - `# expect:` (ordered appearance, prefix match)
    - `# expect_<n>:` (ordered by n, match line starting with output_n..., prefix match)
    - `# expect_<n>_has:` (ordered by n, match line starting with output_n..., substring match)
    """
    items = []
    order_counter = 0
    with path.open() as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# expect"):
                # Match patterns
                parts = line.split(":", 1)
                if len(parts) < 2:
                    continue
                key = parts[0].strip("# ").strip()
                text = parts[1].strip()
                num = None
                mode = "prefix"
                if "_" in key:
                    tail = key.split("_", 1)[1]
                    if tail.endswith("_has"):
                        tail = tail[:-4]
                        mode = "substr"
                    try:
                        num = int(tail)
                    except ValueError:
                        num = None
                items.append((order_counter if num is None else num, num, mode, text))
                order_counter += 1
    # Sort by explicit number first, then by appearance
    items.sort(key=lambda x: x[0])
    return [{"num": num, "mode": mode, "text": text} for _, num, mode, text in items]


def ensure_built():
    """Build the C interpreter and LLVM compiler once before running tests."""
    if not (INTERPRETER.exists() and LLVM_COMPILER.exists()):
        print("Building toolchain (make -C c_using_llvm)...")
    result = subprocess.run(
        ["make", "-C", "c_using_llvm"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        print(result.stdout)
        sys.exit(result.returncode)


def run_interpreter(test_file: Path):
    test_path = test_file.resolve()
    exe = INTERPRETER.resolve()
    proc = subprocess.run(
        [str(exe), str(test_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=exe.parent,
    )
    return proc.returncode, proc.stdout, proc.stderr


def run_llvm(test_file: Path):
    test_path = test_file.resolve()
    compiler_dir = LLVM_COMPILER.parent
    compiler_path = LLVM_COMPILER.resolve()
    with tempfile.TemporaryDirectory() as tmpdir:
        ll_path = Path(tmpdir) / "out.ll"
        bin_path = Path(tmpdir) / "a.out"
        compile_proc = subprocess.run(
            [str(compiler_path), str(test_path), "--emit-llvm", "-o", str(ll_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_dir,
        )
        if compile_proc.returncode != 0:
            return compile_proc.returncode, compile_proc.stdout, compile_proc.stderr

        clang_proc = subprocess.run(
            ["clang", "-O1", "-Wno-override-module", str(ll_path), "runtime.o", "-o", str(bin_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_dir,
        )
        if clang_proc.returncode != 0:
            return clang_proc.returncode, clang_proc.stdout, clang_proc.stderr

        run_proc = subprocess.run(
            [str(bin_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_dir,
        )
        return run_proc.returncode, run_proc.stdout, compile_proc.stderr + clang_proc.stderr + run_proc.stderr


def run_test(test_file: Path, backend: str):
    expected = read_expectations(test_file)
    if not expected:
        return None, "no expectations found"

    if backend == "interpreter":
        code, out, err = run_interpreter(test_file)
    else:
        code, out, err = run_llvm(test_file)

    out = out.rstrip("\n")
    err = err.rstrip("\n")

    if code != 0:
        return False, f"exit {code}\nstderr:\n{err}"
    if err:
        return False, f"unexpected stderr:\n{err}"

    actual_lines = out.splitlines()
    pos = 0
    for exp in expected:
        if exp["num"] is not None:
            patt = re.compile(rf"^output_{exp['num']}\s*[=:]?\s*(.*)$")
            matched = False
            for line in actual_lines:
                m = patt.match(line)
                if not m:
                    continue
                payload = m.group(1)
                if exp["mode"] == "substr":
                    if exp["text"] in payload:
                        matched = True
                        break
                else:
                    if payload.startswith(exp["text"]):
                        matched = True
                        break
            if not matched:
                return False, f"missing expected output_{exp['num']} snippet ({exp['mode']}):\n{exp['text']}\nactual:\n{out}"
        else:
            found = False
            for i in range(pos, len(actual_lines)):
                if actual_lines[i].startswith(exp["text"]):
                    pos = i + 1
                    found = True
                    break
            if not found:
                return False, f"missing expected snippet:\n{exp['text']}\nactual:\n{out}"
    return True, ""


def main():
    parser = argparse.ArgumentParser(description="Run .tl tests with expected output comments.")
    parser.add_argument("--backend", choices=["interpreter", "llvm"], default="interpreter")
    parser.add_argument("--filter", help="Substring filter for test filenames", default="")
    args = parser.parse_args()

    ensure_built()

    tests = sorted(TEST_DIR.glob("*.tl"))
    if args.filter:
        tests = [t for t in tests if args.filter in t.name]

    if not tests:
        print("No tests found.")
        return 1

    failures = []
    skipped = []
    for t in tests:
        ok, msg = run_test(t, args.backend)
        if ok is None:
            skipped.append(t)
            print(f"[SKIP] {t} ({msg})")
        elif ok:
            print(f"[PASS] {t}")
        else:
            print(f"[FAIL] {t}")
            if msg:
                print(msg)
            failures.append(t)

    if failures:
        print(f"\n{len(failures)} test(s) failed.")
        return 1
    passed = len(tests) - len(skipped)
    print(f"\nAll {passed} test(s) passed ({len(skipped)} skipped).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
