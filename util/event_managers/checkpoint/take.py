"""
Create checkpoints at periodic intervals during a simulation.

Checkpoints should presumably be made on a fast core, to be restored on
a more detailed model.
"""

import sys
from pathlib import Path
from typing import Final, Optional

import m5
from gem5.simulate.exit_event import ExitEvent

import util.simarglib as simarglib
from util.event_managers.event_manager import (
    EventHandler,
    EventHandlerDict,
    EventManager,
    EventTime,
)

DEFAULT_CHECKPOINTS_DIR: Final[Path] = Path("checkpoints")

parser = simarglib.add_parser("Periodic Checkpoint Manager")
parser.add_argument(
    "--interval",
    required=True,
    type=int,
    help="Create checkpoint at start of ROI and every INTERVAL million instructions (on core 0) until end of ROI",
)
parser.add_argument(
    "--checkpoints-dir",
    type=Path,
    default=DEFAULT_CHECKPOINTS_DIR,
    help=(
        "The enclosing directory in which to store checkpoints inside "
        f"(default: {DEFAULT_CHECKPOINTS_DIR})"
    ),
)
parser.add_argument(
    "--max-checkpoints", type=int, help="Stop after MAX_CHECKPOINTS checkpoints"
)


class TakeCheckpointsManager(EventManager):
    def __init__(self) -> None:
        """Initialize the TakeCheckpointsManager."""
        super().__init__()

        # count checkpoints taken
        self._checkpoint_num = 0

        # Get the interval
        interval: int = simarglib.get("interval")  # type: ignore
        if interval:
            if self._interval < 1:
                print("INTERVAL must be positive!")
                sys.exit(1)
            interval *= 1000000
        else:
            interval = 0

        self._interval: Final[int] = interval
        self._checkpoints_dir: Final[Path] = simarglib.get("checkpoints_dir")  # type: ignore
        self._max_checkpoints: Final[Optional[int]] = simarglib.get("max_checkpoints")  # type: ignore

        # Create enclosing checkpoint directory
        self._checkpoints_dir.mkdir(parents=True, exist_ok=True)

    def get_event_handlers(self) -> EventHandlerDict:
        """Get a dictionary of event types -> handlers.

        :return A dictionary of this manager's event handlers
        """
        return {
            ExitEvent.MAX_INSTS: self._handle_maxinsts(),
            ExitEvent.WORKBEGIN: self._handle_workbegin(),
            ExitEvent.WORKEND: self._handle_workend(),
        }

    def _handle_maxinsts(self) -> EventHandler:
        """Handle maxinsts event, by taking a new checkpoint.

        :yield True if the simulation should end, false otherwise
        """
        while True:
            self._checkpoint_num += 1
            checkpoint_dir: Path = self._checkpoints_dir / f"chkpt.{str(m5.curTick())}"

            print(f"###Checkpoint {self._checkpoint_num}: {checkpoint_dir}")
            m5.checkpoint(str(checkpoint_dir))

            # If we have more checkpoints to take, keep going, otherwise we're done
            if not self._max_checkpoints or (
                self._checkpoint_num < self._max_checkpoints
            ):
                self.set_next_event(
                    EventTime(instruction=self._interval, cycle=None, tick=None)
                )
                yield False
            else:
                yield True

    def _handle_workbegin(self) -> EventHandler:
        """Handle workbegin event, by taking a new checkpoint.

        :yield True if the simulation should end, false otherwise
        """

        print("===Entering ROI")
        m5.stats.reset()

        print(f"###Taking checkpoints every {self._interval} instructions")
        self._checkpoint_num += 1
        checkpoint_dir: Path = self._checkpoints_dir / f"chkpt.{m5.curTick()}"

        print(f"###Checkpoint {self._checkpoint_num} (start of ROI): {checkpoint_dir}")
        m5.checkpoint(str(checkpoint_dir))

        # If we have more checkpoints to take, keep going, otherwise we're done
        if not self._max_checkpoints or (self._checkpoint_num < self._max_checkpoints):
            self.set_next_event(
                EventTime(instruction=self._interval, cycle=None, tick=None)
            )
            yield False
        else:
            yield True

    def _handle_workend(self) -> EventHandler:
        """Handle workend event, by ending the simulation.

        yield: True if the simulation should end, false otherwise
        """
        print("===Exiting ROI")
        m5.stats.dump()
        m5.stats.reset()

        # End the simulation
        yield True
