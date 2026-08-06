#!/usr/bin/env python3

import json
import subprocess
import sys


MINIMUMS = {
    "functions": 100.0,
    "lines": 90.0,
    "branches": 70.0,
}


def main() -> int:
    if len(sys.argv) < 4:
        print(
            f"usage: {sys.argv[0]} BINARY PROFILE SOURCE [SOURCE ...]",
            file=sys.stderr,
        )
        return 2

    binary, profile, *sources = sys.argv[1:]
    command = [
        "xcrun",
        "llvm-cov",
        "export",
        binary,
        f"-instr-profile={profile}",
        "--summary-only",
        "--sources",
        *sources,
    ]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    totals = json.loads(result.stdout)["data"][0]["totals"]

    failed = False
    summaries = []
    for metric, minimum in MINIMUMS.items():
        actual = totals[metric]["percent"]
        summaries.append(f"{metric} {actual:.2f}%")
        if actual + 1e-9 < minimum:
            print(
                f"coverage check failed: {metric} is {actual:.2f}% "
                f"(minimum {minimum:.2f}%)",
                file=sys.stderr,
            )
            failed = True

    if not failed:
        print("Coverage thresholds met: " + ", ".join(summaries))
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
