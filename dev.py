#!/usr/bin/env python3
import subprocess, sys, argparse
from pathlib import Path
import shutil

ROOT  = Path(__file__).parent
BUILD = ROOT / "out" / "build" / "WSL-GCC-Release"


def cmd_build(args):
    subprocess.run(["cmake", "-S", ".", "-B", str(BUILD), "-DCMAKE_BUILD_TYPE=Release"])
    subprocess.run(["cmake", "--build", str(BUILD), "-j"])

def cmd_bench(args):
    #check BUILD / "bench_mempool" exist
    #check BUILD / "bench_newdelete" exist
    #check BUILD / "bench_tcmalloc" exist
    names = ["bench_mempool", "bench_newdelete", "bench_tcmalloc"]
    for name in names:
        binary = BUILD/name
        if not binary.exists():
            print("[ERROR] target:", binary, "does not exist, Run: python dev.py build")
            sys.exit(1)
        subprocess.run([binary])

def cmd_perf(args):
    names = ["bench_mempool", "bench_newdelete", "bench_tcmalloc"]
    for name in names:
        binary = BUILD/name
        if not binary.exists():
            print("[ERROR] target:", binary, "does not exist, Run: python dev.py build")
            sys.exit(1)
        subprocess.run(["perf", "stat", "-e", "task-clock,context-switches,page-faults", "-r", str(args.repeat), binary])

def cmd_clean(args):
    if BUILD.exists():
        shutil.rmtree(BUILD)
        print(f"[OK] Removed {BUILD}")
    else:
        print("Nothing to clean.")

def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd")
    sub.required = True
    # ... 在这里注册子命令
    p_build = sub.add_parser("build")
    p_build.set_defaults(func=cmd_build)

    p_bench = sub.add_parser("bench")
    p_bench.set_defaults(func=cmd_bench)

    p_perf = sub.add_parser("perf")
    p_perf.add_argument("-r", "--repeat", type=int, default=3)
    p_perf.set_defaults(func=cmd_perf)

    p_clean = sub.add_parser("clean")
    p_clean.set_defaults(func=cmd_clean)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
