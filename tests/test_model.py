import unittest

from linuxpict.model import Annotation, Document, Rect, Tool


class RectTests(unittest.TestCase):
    def test_between_normalizes_reverse_drag(self):
        self.assertEqual(Rect.between(9, 8, 2, 3), Rect(2, 3, 7, 5))

    def test_clamp_intersects_bounds(self):
        self.assertEqual(Rect(-5, 3, 20, 20).clamp(Rect(0, 0, 10, 10)), Rect(0, 3, 10, 7))


class DocumentTests(unittest.TestCase):
    def test_annotations_and_crops_share_undo_stack(self):
        document = Document(100, 80)
        annotation = Annotation(Tool.BOX, (2, 3), (20, 30))
        document.add(annotation)
        document.crop(Rect(10.2, 8.8, 40.1, 30.1))
        self.assertEqual(document.state.crop, Rect(10, 8, 41, 31))
        self.assertTrue(document.undo())
        self.assertEqual(document.state.crop, document.bounds)
        self.assertEqual(document.state.annotations, (annotation,))
        self.assertTrue(document.undo())
        self.assertEqual(document.state.annotations, ())
        self.assertTrue(document.redo())
        self.assertEqual(document.state.annotations, (annotation,))

    def test_crop_is_limited_to_current_crop(self):
        document = Document(100, 80)
        document.crop(Rect(10, 10, 50, 40))
        document.crop(Rect(0, 20, 100, 50))
        self.assertEqual(document.state.crop, Rect(10, 20, 50, 30))

    def test_tiny_crop_is_rejected(self):
        document = Document(100, 80)
        self.assertFalse(document.crop(Rect(1, 1, 1, 1)))
        self.assertEqual(document.state.crop, document.bounds)
