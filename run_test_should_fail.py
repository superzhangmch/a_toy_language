#!/usr/bin/env python3
"""
Run negative tests: every .tl in examples/test_syntax_err/ must fail (parse/compile/runtime).
"""
import argparse
import subprocess
import sys
from pathlib import Path
import tempfile

BASE = Path("examples/test_syntax_err")


def run_interpreter(test_file: Path) -> bool:
    proc = subprocess.run(
        ["c_using_llvm/interpreter", str(test_file.resolve())],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        print(f"[PASS] {test_file.name} (expected failure)")
        if proc.stderr.strip():
            print(f"  stderr: {proc.stderr.strip()}")
        return True
    print(f"[FAIL] {test_file.name} (unexpected success)")
    if proc.stdout.strip():
        print(f"  stdout: {proc.stdout.strip()}")
    if proc.stderr.strip():
        print(f"  stderr: {proc.stderr.strip()}")
    return False


def run_llvm(test_file: Path) -> bool:
    compiler_path = Path("c_using_llvm/codegen_llvm").resolve()
    if not compiler_path.exists():
        print("LLVM compiler not built; run `make -C c_using_llvm` first.")
        return False
    with tempfile.TemporaryDirectory() as tmpdir:
        ll_path = Path(tmpdir) / "out.ll"
        bin_path = Path(tmpdir) / "a.out"
        compile_proc = subprocess.run(
            [str(compiler_path), str(test_file.resolve()), "--emit-llvm", "-o", str(ll_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_path.parent,
        )
        if compile_proc.returncode != 0:
            print(f"[PASS] {test_file.name} (expected fail)")
            if compile_proc.stderr.strip():
                print(f"  stderr: {compile_proc.stderr.strip()}")
            return True

        clang_proc = subprocess.run(
            ["clang", "-O1", "-Wno-override-module", str(ll_path), "runtime.o", "-o", str(bin_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_path.parent,
        )
        if clang_proc.returncode != 0:
            print(f"[PASS] {test_file.name} (expected fail)")
            if clang_proc.stderr.strip():
                print(f"  stderr: {clang_proc.stderr.strip()}")
            return True

        run_proc = subprocess.run(
            [str(bin_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=compiler_path.parent,
        )
        if run_proc.returncode != 0:
            print(f"[PASS] {test_file.name} (expected fail)")
            combined_err = (compile_proc.stderr + clang_proc.stderr + run_proc.stderr)
            err_lines = [line for line in combined_err.splitlines() if line.strip()]
            if err_lines:
                print(f"  stderr: {err_lines[0]}")
            return True

        print(f"[FAIL] {test_file.name} (unexpected success)")
        if compile_proc.stdout.strip():
            print(f"  stdout: {compile_proc.stdout.strip()}")
        if compile_proc.stderr.strip() or clang_proc.stderr.strip() or run_proc.stderr.strip():
            print(f"  stderr: {(compile_proc.stderr + clang_proc.stderr + run_proc.stderr).strip()}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Run negative .tl tests that should fail.")
    parser.add_argument("--backend", choices=["interpreter", "llvm"], default="interpreter")
    args = parser.parse_args()

    tests = sorted(BASE.glob("*.tl"))
    if not tests:
        print("No tests found.")
        return 0

    all_pass = True
    for t in tests:
        if args.backend == "interpreter":
            ok = run_interpreter(t)
        else:
            ok = run_llvm(t)
        all_pass = all_pass and ok

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
