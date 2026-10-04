"""Generate deterministic ASCII glyph bitmaps with Pillow and the bundled font."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1] / "assets/fonts"
scale = 4
for width, height, size, name in [(4, 8, 7, "fine"), (8, 16, 14, "classic")]:
    font = ImageFont.truetype(str(root / "DejaVuSansMono.ttf"), size * scale)
    ascent, descent = font.getmetrics()
    atlas = Image.new("RGBA", (width * 95, height))
    for index in range(95):
        character = chr(32 + index)
        glyph = Image.new("RGBA", (width * scale, height * scale))
        draw = ImageDraw.Draw(glyph)
        x = (width * scale - font.getlength(character)) / 2
        y = (height * scale - ascent - descent) / 2
        draw.text((x, y), character, font=font, fill=(255, 255, 255, 255), anchor="la")
        atlas.paste(glyph.resize((width, height), Image.Resampling.LANCZOS), (index * width, 0))
    atlas.save(root / f"atlas-{name}.png")
