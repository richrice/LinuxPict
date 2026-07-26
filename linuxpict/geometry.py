from __future__ import annotations

from dataclasses import dataclass

from .model import Rect


@dataclass(frozen=True)
class CanvasGeometry:
    crop: Rect
    view_width: float
    view_height: float
    padding: float = 18

    @property
    def scale(self) -> float:
        available_w = max(1, self.view_width - self.padding * 2)
        available_h = max(1, self.view_height - self.padding * 2)
        return min(available_w / self.crop.width, available_h / self.crop.height)

    @property
    def display_rect(self) -> Rect:
        width, height = self.crop.width * self.scale, self.crop.height * self.scale
        return Rect((self.view_width - width) / 2, (self.view_height - height) / 2, width, height)

    def image_to_view(self, x: float, y: float) -> tuple[float, float]:
        display = self.display_rect
        return (
            display.x + (x - self.crop.x) * self.scale,
            display.y + (y - self.crop.y) * self.scale,
        )

    def view_to_image(self, x: float, y: float) -> tuple[float, float]:
        display = self.display_rect
        image_x = self.crop.x + (x - display.x) / self.scale
        image_y = self.crop.y + (y - display.y) / self.scale
        return (
            min(max(image_x, self.crop.x), self.crop.right),
            min(max(image_y, self.crop.y), self.crop.bottom),
        )
