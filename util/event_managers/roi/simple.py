"""
Handle ROIs in a simple manner.

- On any m5 workbegin, switch CPU and begin collecting stats
- On any m5 workend, cease stat collection and switch back to the
  fast-forward processor
"""

import m5
from gem5.simulate.exit_event import ExitEvent

from util.event_managers.event_manager import (
    EventHandler,
    EventHandlerDict,
    EventManager,
)


class SimpleROIManager(EventManager):
    def __init__(self) -> None:
        """Initialize the SimpleROIManager."""
        super().__init__()

    def get_event_handlers(self) -> EventHandlerDict:
        """Get a dictionary of event types -> handlers.

        :return A dictionary of this manager's event handlers
        """
        return {
            ExitEvent.WORKBEGIN: self._handle_workbegin(),
            ExitEvent.WORKEND: self._handle_workend(),
        }

    def _handle_workbegin(self) -> EventHandler:
        """Handle workbegin event, by switching to the timing processor
        and collecting stats.

        :yield True if the simulation should end, false otherwise
        """
        while True:
            print("***Switching to timing processor")
            self.switch_processor()

            print("===Entering ROI")
            m5.stats.reset()

            yield False

    def _handle_workend(self) -> EventHandler:
        """Handle workend event, by switching to the fast-forward
        processor and ceasing stat collection.

        :yield True if the simulation should end, false otherwise
        """
        while True:
            print("===Exiting ROI")
            m5.stats.dump()
            m5.stats.reset()

            print("***Switching to fast-forward processor")
            self.switch_processor()

            yield False
