"""
Create a checkpoint once the OS has booted, then end the simulation.

To be used with the take_boot_checkpoint.py config script.
"""

from pathlib import Path
from typing import Final

import m5
from gem5.simulate.exit_event import ExitEvent

import util.simarglib as simarglib
from util.event_managers.event_manager import (
    EventHandler,
    EventHandlerDict,
    EventManager,
)

DEFAULT_CHECKPOINT_DIR: Final[Path] = Path("boot_checkpoint")

parser = simarglib.add_parser("Post-Boot Checkpoint Manager")
parser.add_argument(
    "--checkpoint-dir",
    type=Path,
    default=DEFAULT_CHECKPOINT_DIR,
    help=(
        "The directory to save the post-boot checkpoint to"
        f"(default: {DEFAULT_CHECKPOINT_DIR})"
    ),
)


class PostBootCheckpointManager(EventManager):
    def __init__(self) -> None:
        """Initialize the PostBootCheckpointManager."""
        self._checkpoint_dir: Final[Path] = Path(simarglib.get("checkpoint_dir"))  # type: ignore

        # Create checkpoint directory
        self._checkpoint_dir.mkdir(parents=True, exist_ok=True)

    def get_event_handlers(self) -> EventHandlerDict:
        """Get a dictionary of event types -> handlers.

        :return A dictionary of this manager's event handlers
        """
        return {ExitEvent.CHECKPOINT: self._handle_checkpoint()}

    def _handle_checkpoint(self) -> EventHandler:
        """Handle checkpoint event, by taking the post-boot checkpoint
        and ending the simulation.

        :yield True if the simulation should end, false otherwise
        """
        print("###Taking post-OS-boot checkpoint")
        m5.checkpoint(str(self._checkpoint_dir))

        # End the simulation
        yield True
