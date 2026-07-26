from __future__ import annotations

from dataclasses import dataclass, replace
from enum import Enum


class Tool(str, Enum):
    ARROW = "arrow"
    BOX = "box"
    ELLIPSE = "ellipse"
    LINE = "line"
    TEXT = "text"
    CROP = "crop"


@dataclass(frozen=True)
class Rect:
    x: float
    y: float
    width: float
    height: float

    @property
    def right(self) -> float:
        return self.x + self.width

    @property
    def bottom(self) -> float:
        return self.y + self.height

    @classmethod
    def between(cls, x1: float, y1: float, x2: float, y2: float) -> "Rect":
        return cls(min(x1, x2), min(y1, y2), abs(x2 - x1), abs(y2 - y1))

    def clamp(self, bounds: "Rect") -> "Rect":
        left, top = max(self.x, bounds.x), max(self.y, bounds.y)
        right, bottom = min(self.right, bounds.right), min(self.bottom, bounds.bottom)
        return Rect(left, top, max(0, right - left), max(0, bottom - top))


@dataclass(frozen=True)
class Annotation:
    tool: Tool
    start: tuple[float, float]
    end: tuple[float, float]
    color: tuple[float, float, float, float] = (1.0, 0.18, 0.12, 1.0)
    width: float = 6.0
    text: str = ""


@dataclass(frozen=True)
class State:
    annotations: tuple[Annotation, ...]
    crop: Rect


class Document:
    def __init__(self, width: int, height: int):
        self.bounds = Rect(0, 0, width, height)
        self.state = State((), self.bounds)
        self._undo: list[State] = []
        self._redo: list[State] = []

    def _commit(self, state: State) -> None:
        if state == self.state:
            return
        self._undo.append(self.state)
        self.state = state
        self._redo.clear()

    def add(self, annotation: Annotation) -> None:
        self._commit(replace(self.state, annotations=self.state.annotations + (annotation,)))

    def crop(self, rect: Rect) -> bool:
        clipped = rect.clamp(self.state.crop)
        if clipped.width < 2 or clipped.height < 2:
            return False
        aligned = Rect(
            int(clipped.x),
            int(clipped.y),
            int(clipped.right + 0.999999) - int(clipped.x),
            int(clipped.bottom + 0.999999) - int(clipped.y),
        )
        self._commit(replace(self.state, crop=aligned))
        return True

    def reset_crop(self) -> None:
        self._commit(replace(self.state, crop=self.bounds))

    def clear(self) -> None:
        self._commit(replace(self.state, annotations=()))

    def undo(self) -> bool:
        if not self._undo:
            return False
        self._redo.append(self.state)
        self.state = self._undo.pop()
        return True

    def redo(self) -> bool:
        if not self._redo:
            return False
        self._undo.append(self.state)
        self.state = self._redo.pop()
        return True
