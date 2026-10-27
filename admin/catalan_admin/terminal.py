"""ANSI styling and keys for terminal output.

Styling is on only for a terminal, and off when NO_COLOR is set, see
https://no-color.org. Piped output stays plain text. Keys are read one at a
time in raw mode, so an arrow works without Enter.
"""

from __future__ import annotations

import os
import select
import sys
import termios
import tty
from typing import TextIO

# clear the screen and move the cursor to the top left
CLEAR = "\033[H\033[2J"


class Style:
    """Wrap text in ANSI escape codes, or pass it through unchanged.

    Args:
        enabled: Whether to emit escape codes.

    """

    def __init__(self, enabled: bool) -> None:
        self.enabled = enabled

    @classmethod
    def for_stream(cls, stream: TextIO) -> Style:
        """Styling for a stream: on for a terminal, unless NO_COLOR is set."""
        return cls(stream.isatty() and not os.environ.get("NO_COLOR"))

    def _wrap(self, code: str, text: str) -> str:
        return f"\033[{code}m{text}\033[0m" if self.enabled else text

    def bold(self, text: str) -> str:
        """Bold, for titles and day headings."""
        return self._wrap("1", text)

    def dim(self, text: str) -> str:
        """Dimmed, for times, authors, and rules."""
        return self._wrap("2", text)

    def id(self, text: str) -> str:
        """Bold cyan, for post ids, what show and remove take."""
        return self._wrap("1;36", text)

    def warning(self, text: str) -> str:
        """Yellow, for unlisted posts."""
        return self._wrap("33", text)

    def link(self, text: str) -> str:
        """Magenta, for the post an edit is of."""
        return self._wrap("35", text)


def decode_key(sequence: str) -> str:
    """Name the action for a keypress.

    Args:
        sequence: The characters one keypress sent.

    Returns:
        "next", "previous", "refresh", "quit", or "" for any other key.

    Examples:

        decode_key("\\033[B")  # "next", the down arrow
        decode_key("k")        # "previous"
        decode_key("r")        # "refresh"
        decode_key("x")        # ""

    """
    if sequence in ("\033[B", "\033OB", "\r", "\n", " ", "j"):
        return "next"
    if sequence in ("\033[A", "\033OA", "k"):
        return "previous"
    if sequence in ("r", "R"):
        return "refresh"
    if sequence in ("q", "Q", "\033", "\x03"):
        return "quit"
    return ""


def read_key() -> str:
    """Read one keypress from the terminal and name its action.

    The terminal is in raw mode for the one keypress, so it arrives without
    Enter. An arrow sends three characters, an escape and then "[A" or "[B".
    After an escape, the rest is read only if it arrives within 50 ms, so Esc
    alone still quits.

    Returns:
        "next", "previous", "refresh", "quit", or "" for any other key.

    """
    descriptor = sys.stdin.fileno()
    saved = termios.tcgetattr(descriptor)
    try:
        # TCSANOW keeps keys pressed before the call, TCSAFLUSH would drop them
        tty.setraw(descriptor, termios.TCSANOW)
        sequence = os.read(descriptor, 1).decode(errors="replace")
        if sequence == "\033":
            while select.select([descriptor], [], [], 0.05)[0]:
                sequence += os.read(descriptor, 1).decode(errors="replace")
                if len(sequence) == 3:
                    break
    finally:
        termios.tcsetattr(descriptor, termios.TCSADRAIN, saved)
    return decode_key(sequence)
