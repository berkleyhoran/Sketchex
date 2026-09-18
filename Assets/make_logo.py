"""Generates Assets/logo.png (1024px) and Assets/logo.ico for Sketchex:
a rounded sky-to-pink gradient tile with a glowing hand-drawn melody line
and a playhead. Pure Pillow, no external assets."""
import math
from PIL import Image, ImageDraw, ImageFilter

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

# Background tile: rounded rect with a vertical gradient.
bg = Image.new("RGBA", (S, S), (0, 0, 0, 0))
grad = Image.new("RGBA", (S, S))
gd = ImageDraw.Draw(grad)
top, bot = (221, 241, 255), (246, 233, 255)
for y in range(S):
    t = y / (S - 1)
    c = tuple(int(top[i] * (1 - t) + bot[i] * t) for i in range(3)) + (255,)
    gd.line([(0, y), (S, y)], fill=c)
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([40, 40, S - 40, S - 40], radius=220, fill=255)
bg.paste(grad, (0, 0), mask)

# Soft aurora blobs.
blobs = Image.new("RGBA", (S, S), (0, 0, 0, 0))
bd = ImageDraw.Draw(blobs)
for (cx, cy, col) in [(300, 300, (62, 200, 255, 110)), (760, 340, (255, 95, 168, 90)),
                      (520, 780, (124, 255, 142, 90)), (820, 760, (255, 197, 62, 80))]:
    r = 320
    bd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
blobs = blobs.filter(ImageFilter.GaussianBlur(120))
bg = Image.alpha_composite(bg, Image.composite(blobs, Image.new("RGBA", (S, S), (0, 0, 0, 0)), mask))

# Faint lane lines.
ld = ImageDraw.Draw(bg)
for i in range(1, 8):
    y = 120 + i * 100
    ld.line([(140, y), (S - 140, y)], fill=(28, 35, 64, 28), width=3)

# The melody line.
pts = []
for i in range(0, 121):
    t = i / 120
    x = 150 + t * (S - 300)
    y = 540 - 240 * math.sin(t * math.pi * 1.6 + 0.3) - 60 * math.sin(t * math.pi * 5)
    pts.append((x, y))

def stroke_layer(width, colour, blur):
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.line(pts, fill=colour, width=width, joint="curve")
    r = width // 2
    for p in (pts[0], pts[-1]):
        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=colour)
    return layer.filter(ImageFilter.GaussianBlur(blur)) if blur else layer

sky = (62, 200, 255)
bg = Image.alpha_composite(bg, stroke_layer(110, sky + (120,), 40))
bg = Image.alpha_composite(bg, stroke_layer(54, sky + (200,), 8))
bg = Image.alpha_composite(bg, stroke_layer(26, (140, 230, 255, 255), 0))
bg = Image.alpha_composite(bg, stroke_layer(9, (255, 255, 255, 230), 0))

# Playhead.
px = 640
ph = Image.new("RGBA", (S, S), (0, 0, 0, 0))
pd = ImageDraw.Draw(ph)
pd.rectangle([px - 22, 120, px + 22, S - 120], fill=(255, 95, 168, 90))
ph = ph.filter(ImageFilter.GaussianBlur(14))
bg = Image.alpha_composite(bg, ph)
pd = ImageDraw.Draw(bg)
pd.rectangle([px - 7, 120, px + 7, S - 120], fill=(255, 95, 168, 255))
pd.rectangle([px - 2, 120, px + 2, S - 120], fill=(255, 255, 255, 220))
pd.polygon([(px - 46, 96), (px + 46, 96), (px, 170)], fill=(255, 95, 168, 255))

# Note-hit dot where the playhead crosses the line.
hy = min(pts, key=lambda p: abs(p[0] - px))[1]
dot = Image.new("RGBA", (S, S), (0, 0, 0, 0))
dd = ImageDraw.Draw(dot)
dd.ellipse([px - 70, hy - 70, px + 70, hy + 70], fill=(255, 255, 255, 160))
dot = dot.filter(ImageFilter.GaussianBlur(24))
bg = Image.alpha_composite(bg, dot)
pd = ImageDraw.Draw(bg)
pd.ellipse([px - 30, hy - 30, px + 30, hy + 30], fill=(255, 255, 255, 255))
pd.ellipse([px - 30, hy - 30, px + 30, hy + 30], outline=(255, 95, 168, 255), width=10)

# Edge.
pd.rounded_rectangle([40, 40, S - 40, S - 40], radius=220, outline=(28, 35, 64, 70), width=6)

bg.save("Assets/logo.png")
bg.resize((256, 256), Image.LANCZOS).save("Assets/logo.ico", sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
print("wrote Assets/logo.png + logo.ico")
