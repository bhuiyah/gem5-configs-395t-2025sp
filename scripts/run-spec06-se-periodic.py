#!/usr/bin/env python3

"""Run a SPEC '06 benchmark in a gem5 SE-mode simulation with periodic ROIs."""

import argparse
from pathlib import Path
from typing import Final, List

from util.spec import DEFAULT_SPEC06_DIR, SPEC06_BENCHMARKS, SpecCommand

DEFAULT_GEM5_BINARY: Final[Path] = Path("../") / "gem5" / "build" / "X86" / "gem5.opt"
DEFAULT_OUTDIR: Final[Path] = Path("m5out")


def get_args() -> argparse.Namespace:
    """Get the arguments for this script."""
    parser = argparse.ArgumentParser(
        "Helper script to run gem5 simulations on SPEC '06 benchmarks"
    )
    parser.add_argument(
        "benchmark",
        type=str,
        choices=SPEC06_BENCHMARKS,
        help="The SPEC '06 benchmark to run",
    )
    parser.add_argument(
        "-g",
        "--gem5-binary",
        type=Path,
        default=DEFAULT_GEM5_BINARY,
        help=f"The gem5 binary to use (default: {DEFAULT_GEM5_BINARY})",
    )
    parser.add_argument(
        "-o",
        "--outdir",
        type=Path,
        default=DEFAULT_OUTDIR,
        help=(
            "Path to save gem5 output files to. A benchmark's runs "
            "will be saved under <outdir>/<benchmark> (default: "
            f"{DEFAULT_OUTDIR})"
        ),
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

    # se_custom_binary_periodic.py arguments
    parser.add_argument(
        "--redirect",
        action="store_true",
        help=(
            "Redirect stdout and stderr to a file. This is equivalent "
            "to passing -re to the gem5 binary."
        ),
    )
    parser.add_argument(
        "--ff-interval",
        default=None,
        help="The fast-forward interval to pass to se_custom_binary_periodic.py",
    )
    parser.add_argument(
        "--warmup-interval",
        default=None,
        help="The warmup interval to pass to se_custom_binary_periodic.py",
    )
    parser.add_argument(
        "--roi-interval",
        default=None,
        help="The ROI interval to pass to se_custom_binary_periodic.py",
    )
    parser.add_argument(
        "--init-ff-interval",
        default=None,
        help=(
            "The initial fast-forward interval to pass to "
            "se_custom_binary_periodic.py"
        ),
    )
    parser.add_argument(
        "--num-rois",
        default=None,
        help="The number of ROIs to run. If not specified, run indefinitely.",
    )
    parser.add_argument(
        "--continue-sim",
        action="store_true",
        help=(
            "After the last ROI finishes, continue fast-forward executio "
            "until the binary finishes."
        ),
    )
    return parser.parse_args()


def main():
    """Run a SPEC 2006 benchmark in gem5 SE-mode with periodic ROIs."""
    args = get_args()

    print("run-spec06-se-periodic")
    print(f"benchmark  : {args.benchmark}")
    print(f"gem5_binary: {args.gem5_binary}")
    print(f"outdir     : {args.outdir}")
    print(f"spec06_dir : {args.spec06_dir}")
    print(f"redirect   : {args.redirect}")
    print()
    print("se_custom_binary_periodic.py arguments:")
    print(f"ff_interval     : {args.ff_interval}")
    print(f"warmup_interval : {args.warmup_interval}")
    print(f"roi_interval    : {args.roi_interval}")
    print(f"init_ff_interval: {args.init_ff_interval}")
    print(f"num_rois        : {args.num_rois}")
    print(f"continue_sim    : {args.continue_sim}")
    print()

    spec_command: Final[SpecCommand] = SpecCommand(args.benchmark, args.spec06_dir)

    gem5_binary_args: List[str] = [
        f"--outdir={(args.outdir / args.benchmark).absolute()}"
    ]
    if args.redirect:
        gem5_binary_args.append("-re")

    gem5_script_args: List[str] = [
        f"--input-bin={spec_command.bin}",
        f"--input-args=\"{' '.join(spec_command.args)}\"",
    ]
    if args.ff_interval is not None:
        gem5_script_args.append(f"--ff-interval={args.ff_interval}")
    if args.warmup_interval is not None:
        gem5_script_args.append(f"--warmup-interval={args.warmup_interval}")
    if args.roi_interval is not None:
        gem5_script_args.append(f"--roi-interval={args.roi_interval}")
    if args.init_ff_interval is not None:
        gem5_script_args.append(f"--init-ff-interval={args.init_ff_interval}")
    if args.num_rois is not None:
        gem5_script_args.append(f"--num-rois={args.num_rois}")
    if args.continue_sim:
        gem5_script_args.append("--continue-sim")

    spec_command.simulate(
        args.gem5_binary,
        gem5_binary_args,
        Path("./se_custom_binary_periodic.py"),
        gem5_script_args,
    )


if __name__ == "__main__":
    main()
