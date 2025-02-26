"""
Hello world in syscall emulation
"""

import sys
from pathlib import Path

from gem5.resources.resource import BinaryResource

import util.simarglib as simarglib
from workloads.custom_workloads import CustomSEWorkload


class HelloWorldSE(CustomSEWorkload):
    def __init__(self) -> None:
        if simarglib.get("start_from"):
            print("--start-from not supported by HelloWorldSE workload")
            sys.exit(1)

        # Get the path to the gem5 "hello" test binary.
        gem5_binary = Path(sys.executable)
        gem5_home = gem5_binary.parent.parent.parent
        gem5_hello_world = (
            gem5_home
            / "tests"
            / "test-progs"
            / "hello"
            / "bin"
            / "x86"
            / "linux"
            / "hello"
        )

        super().__init__(binary=BinaryResource(str(gem5_hello_world)))
