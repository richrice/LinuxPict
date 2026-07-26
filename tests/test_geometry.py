import unittest

from linuxpict.geometry import CanvasGeometry
from linuxpict.model import Rect


class CanvasGeometryTests(unittest.TestCase):
    def test_fit_and_round_trip_with_crop(self):
        geometry = CanvasGeometry(Rect(100, 50, 400, 200), 1000, 700, padding=0)
        self.assertEqual(geometry.scale, 2.5)
        self.assertEqual(geometry.display_rect, Rect(0, 100, 1000, 500))
        view = geometry.image_to_view(300, 150)
        self.assertEqual(view, (500, 350))
        self.assertEqual(geometry.view_to_image(*view), (300, 150))

    def test_view_point_is_clamped_to_visible_image(self):
        geometry = CanvasGeometry(Rect(10, 20, 100, 50), 200, 200, padding=0)
        self.assertEqual(geometry.view_to_image(-100, -100), (10, 20))
        self.assertEqual(geometry.view_to_image(500, 500), (110, 70))
