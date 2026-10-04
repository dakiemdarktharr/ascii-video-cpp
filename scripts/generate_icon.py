"""Regenerate the app's native icon assets (requires Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1] / "assets" / "icons"
root.mkdir(parents=True, exist_ok=True)
image = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((32, 32, 992, 992), radius=206, fill="#355aa2")
# A play symbol built from character cells, with a terminal prompt below.
cells = [(3, 1), (3, 2), (4, 2), (3, 3), (4, 3), (5, 3),
         (3, 4), (4, 4), (5, 4), (6, 4), (3, 5), (4, 5), (5, 5),
         (3, 6), (4, 6), (3, 7)]
rectangles = []
for x, y in cells:
    left, top = 100 + x * 90, 140 + y * 70
    rectangles.append((left, top, 56, 46))
    draw.rectangle((left, top, left + 56, top + 46), fill="#ffffff")
draw.line((252, 749, 295, 789, 252, 829), fill="#b3dbed", width=18)
draw.line((345, 824, 440, 824), fill="#b3dbed", width=18)
image.resize((512, 512), Image.Resampling.LANCZOS).save(root / "app.png")
image.save(root / "app.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
image.save(root / "app.icns")
svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024">\n'
svg += '<rect x="32" y="32" width="960" height="960" rx="206" fill="#355aa2"/>\n'
svg += ''.join(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="white"/>\n' for x, y, w, h in rectangles)
svg += '<path d="M252 749L295 789L252 829M345 824H440" fill="none" stroke="#b3dbed" stroke-width="18"/>\n</svg>\n'
(root / "app.svg").write_text(svg, encoding="utf-8")
