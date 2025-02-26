#!/usr/bin/env python3

"""Run a SPEC '06 benchmark on the host system."""

import argparse
from pathlib import Path
from typing import Final

from util.spec import DEFAULT_SPEC06_DIR, SPEC06_BENCHMARKS, SpecCommand


def get_args() -> argparse.Namespace:
    """Get the arguments for this script."""
    parser = argparse.ArgumentParser(
        "Helper script to run SPEC '06 benchmarks on the host system"
    )
    parser.add_argument(
        "benchmark",
        type=str,
        choices=SPEC06_BENCHMARKS,
        help="The SPEC '06 benchmark to run",
    )
    parser.add_argument(
        "-s",
        "--spec06-dir",
        type=Path,
        default=DEFAULT_SPEC06_DIR,
        help=(
            "The directory containing your copy of the SPEC '06 benchmarks"
            f"(default: {DEFAULT_SPEC06_DIR})"
        ),
    )
    return parser.parse_args()


def main():
    """Run a SPEC 2006 benchmark."""
    args = get_args()

    print("run-spec06-host")
    print(f"benchmark : {args.benchmark}")
    print(f"spec06_dir: {args.spec06_dir}")
    print()

    spec_command: Final[SpecCommand] = SpecCommand(args.benchmark, args.spec06_dir)
    spec_command.run()


if __name__ == "__main__":
    main()
