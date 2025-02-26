"""
Restores simulation from a given checkpoint and runs for X million warmup
instructions and then collects ROI stats for Y million instructions.
No processor switching, so restore should be with a detailed timing core

To be used with the restore_checkpoint.py config script.
"""

import sys
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

DEFAULT_WARMUP_INTERVAL: Final[float] = 200.0  # M instructions
DEFAULT_ROI_INTERVAL: Final[float] = 800.0  # M instructions

parser = simarglib.add_parser("Restore Checkpoint Manager")
parser.add_argument(
    "--warmup-interval",
    type=float,
    default=DEFAULT_WARMUP_INTERVAL,
    help=(
        "The number of instructions to warm up for, after restoring "
        f"the checkpoint (defualt: {DEFAULT_WARMUP_INTERVAL} M "
        "instructions)"
    ),
)
parser.add_argument(
    "--roi-interval",
    type=float,
    default=DEFAULT_ROI_INTERVAL,
    help=(
        "The number of instructions to simulate and record stats for, "
        f"after the warmup phase (default: {DEFAULT_ROI_INTERVAL} M "
        "instructions)"
    ),
)


class RestoreCheckpointManager(EventManager):
    def __init__(self) -> None:
        super().__init__()

        warmup_interval: Optional[float] = simarglib.get("warmup")  # type: ignore
        roi_interval: Optional[float] = simarglib.get("roi")  # type: ignore

        if warmup_interval:
            if warmup_interval < 0.0:
                print("WARMUP must be non-negative!")
                sys.exit(1)
            warmup_interval *= 1_000_000.0
        else:
            warmup_interval = 0.0

        if roi_interval:
            if roi_interval < 1:
                print("ROI must be positive!")
                sys.exit(1)
            roi_interval *= 1_000_000.0
        else:
            roi_interval = 0.0

        self._warmup_interval: Final[int] = int(warmup_interval)
        self._roi_interval: Final[int] = int(roi_interval)

    def initialize(self) -> None:
        """Initialize the checkpoint manager.

        We need to set up initial max insts interrupts and start ROI if
        we're not in warmup. Since the board is not initialized yet, we
        must pass a flag to that effect to schedule_max_insts().
        """
        if self._warmup_interval > 0:
            self.set_next_event(
                EventTime(
                    instruction=self._warmup_interval,
                    cycle=None,
                    tick=None,
                )
            )
        else:
            print("===Entering ROI at restore")
            m5.stats.reset()

            if self._roi_interval > 0:
                self.set_next_event(
                    EventTime(
                        instruction=self._roi_interval,
                        cycle=None,
                        tick=None,
                    )
                )

    def get_event_handlers(self) -> EventHandlerDict:
        """Get dictionary of event types -> handlers.

        :return A dictionary of this manager's event handlers
        """
        return {
            ExitEvent.MAX_INSTS: self._handle_maxinsts(),
            ExitEvent.WORKEND: self._handle_workend(),
        }

    def _handle_workend(self) -> EventHandler:
        """Handle workend event by ending the ROI.

        :yield True if the simulation should end, false otherwise
        """
        print("===Exiting ROI at workend")
        m5.stats.dump()
        m5.stats.reset()

        # End the simulation
        yield True

    def _handle_maxinsts(self) -> EventHandler:
        """Handle maxinsts event by changing the phase.

        :yield True if the simulation should end, false otherwise
        """
        # The first maxinsts event indicates the end of the warmup
        # phase, so we should enter the ROI phase and continue the
        # simulation
        if self._warmup_interval > 0:
            print("===Entering ROI at end of warmup")
            m5.stats.reset()

            if self._roi_interval > 0:
                self.set_next_event(
                    EventTime(
                        instruction=self._roi_interval,
                        cycle=None,
                        tick=None,
                    )
                )

            # Continue the simulation
            yield False

        # The second maxinsts event indicates the end of the ROI phase,
        # so we end stop the simulation
        print("===Exiting ROI after max insts")
        m5.stats.dump()
        m5.stats.reset()

        # End the simulation
        yield True
