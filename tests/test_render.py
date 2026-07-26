import os
import tempfile
import unittest

import cairo

from linuxpict.model import Annotation, Rect, Tool
from linuxpict.render import render_png


class RenderTests(unittest.TestCase):
    def test_export_preserves_crop_dimensions(self):
        with tempfile.TemporaryDirectory() as directory:
            source_path = os.path.join(directory, "source.png")
            output_path = os.path.join(directory, "output.png")
            source = cairo.ImageSurface(cairo.FORMAT_ARGB32, 120, 90)
            context = cairo.Context(source)
            context.set_source_rgb(0.2, 0.4, 0.6)
            context.paint()
            source.write_to_png(source_path)
            render_png(
                source_path,
                output_path,
                (Annotation(Tool.ARROW, (20, 20), (60, 40)),),
                Rect(10, 15, 80, 50),
            )
            result = cairo.ImageSurface.create_from_png(output_path)
            self.assertEqual((result.get_width(), result.get_height()), (80, 50))
