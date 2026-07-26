from __future__ import annotations

import math

import cairo

from .model import Annotation, Rect, Tool


def draw_annotation(ctx: cairo.Context, item: Annotation) -> None:
    x1, y1 = item.start
    x2, y2 = item.end
    ctx.save()
    ctx.set_source_rgba(*item.color)
    ctx.set_line_width(item.width)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    if item.tool == Tool.LINE:
        ctx.move_to(x1, y1)
        ctx.line_to(x2, y2)
        ctx.stroke()
    elif item.tool == Tool.ARROW:
        ctx.move_to(x1, y1)
        ctx.line_to(x2, y2)
        angle = math.atan2(y2 - y1, x2 - x1)
        head = max(14, item.width * 3)
        for offset in (-0.55, 0.55):
            ctx.move_to(x2, y2)
            ctx.line_to(x2 - head * math.cos(angle + offset), y2 - head * math.sin(angle + offset))
        ctx.stroke()
    elif item.tool == Tool.BOX:
        rect = Rect.between(x1, y1, x2, y2)
        ctx.rectangle(rect.x, rect.y, rect.width, rect.height)
        ctx.stroke()
    elif item.tool == Tool.ELLIPSE:
        rect = Rect.between(x1, y1, x2, y2)
        ctx.translate(rect.x + rect.width / 2, rect.y + rect.height / 2)
        ctx.scale(max(rect.width / 2, 0.001), max(rect.height / 2, 0.001))
        ctx.arc(0, 0, 1, 0, math.tau)
        ctx.set_line_width(item.width / max(rect.width / 2, rect.height / 2, 0.001))
        ctx.stroke()
    elif item.tool == Tool.TEXT and item.text:
        ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        ctx.set_font_size(max(16, item.width * 4))
        for line_no, line in enumerate(item.text.splitlines()):
            ctx.move_to(x1, y1 + max(16, item.width * 4) * (line_no + 1))
            ctx.show_text(line)
    ctx.restore()


def render_png(source_path: str, output_path: str, annotations: tuple[Annotation, ...], crop: Rect) -> None:
    source = cairo.ImageSurface.create_from_png(source_path)
    width, height = int(crop.width), int(crop.height)
    target = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
    ctx = cairo.Context(target)
    ctx.set_source_surface(source, -crop.x, -crop.y)
    ctx.paint()
    ctx.translate(-crop.x, -crop.y)
    for item in annotations:
        draw_annotation(ctx, item)
    target.write_to_png(output_path)
