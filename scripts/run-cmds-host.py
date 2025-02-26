#!/usr/bin/env python3

"""Run a list of commands from a .txt file on this machine in parallel.

They will not print to the terminal, so you'll have to redirect the
stdout/stderr output yourself, either by adding the "--re" flag after
the "--outdir" flag (for gem5) or adding a shell redirect (">", for
other programs) to your command.

Each line in the file represents one command.
An example is provided in run-cmds-host-sample.txt.
"""

import argparse
import multiprocessing
import subprocess
from pathlib import Path
from typing import List


def run_command(cmd: str):
    """Run a single command.

    :param cmd The command to run.
    """
    print(f'Running command: "{cmd.rstrip()}"')
    # Run command silently (except for errors)
    res = subprocess.run(
        cmd,
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    ).returncode

    # Check for error
    if res != 0:
        print(f'Command failed: "{cmd.rstrip()}" (error code {res}).')
    else:
        print(f'Command completed: "{cmd.rstrip()}"')


def run_commands_parallel(cmds: List[str], num_workers: int = 8):
    """Run a series of commands in parallel.

    :param cmds The commands to run.
    :param num_workers The number of parallel workers to use
    """
    # Run the commands with a multiprocessing pool
    with multiprocessing.Pool(num_workers) as pool:
        pool.map(run_command, cmds)


def read_command_file(command_file: Path) -> List[str]:
    """Read the commands from a command .txt file

    :param command_file The path to the command file
    :return The list of commands
    :raise FileNotFoundError If the file does not exist
    """
    # Read the file
    if not command_file.exists():
        raise FileNotFoundError(f"Command file {command_file} does not exist.")

    commands: List[str] = []
    with command_file.open("rt") as f:
        commands: List[str] = f.readlines()
        return commands


def get_args() -> argparse.Namespace:
    """Get the arguments to this script.

    :return The parsed arguments.
    """
    parser = argparse.ArgumentParser(
        "Run a list of commands from a .txt file on this machine in parallel."
    )
    parser.add_argument(
        "command-file",
        type=Path,
        required=True,
        help="Path to the file containing the commands.",
    )
    parser.add_argument(
        "--num-workers",
        type=int,
        default=1,
        help="The number of threads to run jobs in parallel.",
    )
    return parser.parse_args()


def main():
    """Run this script."""
    args = get_args()

    commands = read_command_file(args.command_file)
    run_commands_parallel(commands, args.num_workers)


if __name__ == "__main__":
    main()
