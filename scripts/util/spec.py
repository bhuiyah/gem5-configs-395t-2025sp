"""Utilities for working with SPEC06 benchmarks."""

import os
import subprocess
from pathlib import Path
from typing import Final, List, Optional

# List of SPEC06 benchmarks
SPEC06_BENCHMARKS: Final[List[str]] = [
    "401.bzip2",
    "403.gcc",
    "410.bwaves",
    "429.mcf",
    "433.milc",
    "434.zeusmp",
    "436.cactusADM",
    "437.leslie3d",
    "450.soplex",
    "454.calculix",
    "456.hmmer",
    "459.GemsFDTD",
    "462.libquantum",
    "464.h264ref",
    "465.tonto",
    "470.lbm",
    "471.omnetpp",
    "473.astar",
    "482.sphinx3",
    "483.xalancbmk",
]

# Default path to the SPEC06 benchmarks
DEFAULT_SPEC06_DIR: Final[Path] = (
    Path("/")
    / "projects"
    / "coursework"
    / "2025-spring"
    / "cs395t-lin"
    / str(os.getenv("USER"))
    / "spec06"
)


def get_specrun_file(benchmark: str, spec_dir: Path) -> Optional[Path]:
    """Get the path to a benchmark's specrun script.

    :param benchmark The name of the benchmark
    :return The path to the specrun script, if found
    """
    for child in spec_dir.iterdir():
        if child.name == benchmark and child.is_dir():
            return child / "specrun.sh"
    return None


def get_specrun_command(specrun_file: Path) -> Optional[List[str]]:
    """Get the command to run a SPEC benchmark.

    :param specrun_file The path to the specrun script
    :return The command to run the benchmark
    """
    with open(specrun_file, "rt") as file:
        for line in file:
            line = line.strip()
            line_tokens = line.split()

            # Return the first line with a valid command. specrun_sh
            # commands are expected to be on a single-line.
            if not line.startswith("#") and len(line_tokens) > 0:
                return line_tokens

    return None


class SpecCommand:
    def __init__(self, benchmark: str, spec_dir: Path) -> None:
        """Initialize the command.

        :param benchmark The name of the benchmark
        :param spec_dir The directory containing the SPEC benchmarks
        """
        # Get the path to the specrun.sh file
        specrun_file: Final[Optional[Path]] = get_specrun_file(benchmark, spec_dir)
        if specrun_file is None:
            raise FileNotFoundError(f"Could not find specrun.sh for {benchmark}")

        # Get the parent directory of the specrun.sh file.
        specrun_cwd: Final[Path] = specrun_file.parent

        # Get the command inside the specrun.sh file
        specrun_command: Final[Optional[List[str]]] = get_specrun_command(specrun_file)
        if specrun_command is None:
            raise ValueError(f"Could not find command in {specrun_file}")
        if len(specrun_command) == 0:
            raise ValueError(f"Empty command in {specrun_file}")

        # Adjust file paths
        parsed_command: List[str] = specrun_command.copy()
        for token in parsed_command:
            if token != "." and (specrun_cwd / token).exists():
                parsed_command[parsed_command.index(token)] = str(specrun_cwd / token)

        self.benchmark: Final[str] = benchmark
        self.bin: Final[Path] = Path(parsed_command[0])
        self.cwd: Final[Path] = specrun_cwd
        self.args: List[str] = parsed_command[1:] if len(parsed_command) > 1 else []

    def __str__(self) -> str:
        return (
            f"SpecCommand(benchmark={self.benchmark}, bin={self.bin}, args={self.args})"
        )

    def run(self) -> int:
        """Run the benchmark directly.

        :return The return code of the launched process
        """
        spec_command: Final[List[str]] = [
            f"{self.bin.absolute()}",
            *self.args,
        ]

        spec_process = subprocess.Popen(
            " ".join(spec_command),
            cwd=self.cwd,
            shell=True,
        )

        try:
            spec_process.wait()
        except KeyboardInterrupt:
            print()
            spec_process.wait()

        return spec_process.returncode

    def simulate(
        self,
        gem5_binary: Path,
        gem5_binary_args: List[str],
        gem5_script: Path,
        gem5_script_args: List[str],
    ):
        """Run the benchmark in a gem5 simulation.

        :param gem5_binary The path to the gem5 binary
        :param gem5_binary_args Arguments to the gem5 binary
        :param gem5_script The path to the gem5 config script
        :param gem5_script_args Arguments to the gem5 config script
        :return The return code of the launched process
        """
        gem5_command: Final[List[str]] = [
            f"{gem5_binary.absolute()}",
            *gem5_binary_args,
            "--",
            f"{gem5_script.absolute()}",
            *gem5_script_args,
        ]

        print(f"Using cwd: {self.cwd}")
        print(f"Running command: {' '.join(gem5_command)}")

        gem5_process = subprocess.Popen(
            " ".join(gem5_command),
            cwd=self.cwd,
            shell=True,
        )

        try:
            gem5_process.wait()
        except KeyboardInterrupt:
            print()
            gem5_process.wait()

        return gem5_process.returncode
