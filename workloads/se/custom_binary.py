"""
Syscall emulation workload to run an arbitrary input binary and its arguments
"""

import sys
from pathlib import Path
from typing import Final

from gem5.resources.resource import BinaryResource

import util.simarglib as simarglib
from workloads.custom_workloads import CustomSEWorkload

parser = simarglib.add_parser("Custom Binary SE Workload")
parser.add_argument(
    "--input-bin",
    required=True,
    type=Path,
    help="The input binary to simulate [REQUIRED]",
)
parser.add_argument(
    "--input-args",
    type=str,
    help="The arguments to provide the binary to simulate (NOTE: multiple arguments must be wrapped in quotes!)",
)


class CustomBinarySE(CustomSEWorkload):
    def __init__(self) -> None:
        inbin: Final[Path] = simarglib.get("input_bin").absolute()  # type: ignore
        inargs: Final[str] = simarglib.get("input_args")  # type: ignore

        if simarglib.get("start_from"):
            print("--start_from not supported by CustomBinarySE Workload")
            sys.exit(1)

        if not inbin.exists():
            print(f"Input binary {inbin} does not exist!")
            sys.exit(1)

        super().__init__(
            binary=BinaryResource(str(inbin)),
            arguments=inargs.split() if inargs else [],
        )
