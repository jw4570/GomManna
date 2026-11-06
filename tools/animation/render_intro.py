"""Render the GomManna Korean wordmark animation. Requires Pillow and ffmpeg.

Run with a Python environment containing Pillow. Uses the macOS Korean system font;
font files are never copied into the output. IntroView uses the exported glyph masks;
MOV/WebM/GIF files are previews for external use.
"""
from pathlib import Path
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "GomManna/Resources/Animations"
FONT = "/System/Library/Fonts/AppleSDGothicNeo.ttc"
W, H, FPS, DURATION = 1080, 720, 30, 4.2
PLAYBACK_SPEED = 1.2
INITIAL_HOLD = 1.0
SCALE = 2
COCOA = (110, 74, 54)
THEMES = {
    "light": {"text": COCOA},
    "dark": {"text": (214, 168, 122)},
}
BASE_FONT = ImageFont.truetype(FONT, 96 * SCALE, index=6)


def ease(value):
    value = max(0, min(1, value))
    return value * value * (3 - 2 * value)


def phase(t, start, end):
    return ease((t - start) / (end - start))


def mix(a, b, amount):
    return a + (b - a) * amount


def render(t):
    t = max(0, t - INITIAL_HOLD) * PLAYBACK_SPEED
    canvas = Image.new("RGBA", (W * SCALE, H * SCALE))
    entrance = 1
    remove = phase(t, 0.15, 1.00)
    morph = phase(t, 1.00, 1.65)
    join = phase(t, 1.65, 2.55)
    font = BASE_FONT
    measure = lambda text: font.getlength(text) / SCALE
    left, go, gap, meet, right = [measure(s) for s in ["우리", "곧", " ", "만나", "요"]]
    full = left + gap + go + gap + meet + right
    initial_x = (W - full) / 2
    core_x = initial_x + left + gap
    final_x = (W - go - meet) / 2
    x_go = mix(core_x, final_x, join)
    x_meet = x_go + go + gap * (1 - join)
    baseline = 402

    def text(value, x, y, color, opacity=1, font_override=None):
        layer = Image.new("RGBA", canvas.size)
        ImageDraw.Draw(layer).text(
            (round(x * SCALE), round(y * SCALE)), value, font=font_override or font,
            fill=(*color, round(255 * max(0, min(1, opacity * entrance)))), anchor="ls",
        )
        canvas.alpha_composite(layer)

    # The two precomposed syllables have the same top consonant/vowel. Blending
    # aligned glyphs changes the final consonant ㄷ → ㅁ while keeping 고 stable.
    before = Image.new("L", canvas.size)
    after = Image.new("L", canvas.size)
    position = (round(x_go * SCALE), round(baseline * SCALE))
    ImageDraw.Draw(before).text(position, "곧", font=font, fill=255, anchor="ls")
    ImageDraw.Draw(after).text(position, "곰", font=font, fill=255, anchor="ls")
    glyph_mask = Image.blend(before, after, morph).point(lambda alpha: round(alpha * entrance))
    glyph = Image.new("RGBA", canvas.size, (*COCOA, 255))
    glyph.putalpha(glyph_mask)
    canvas.alpha_composite(glyph)
    text("만나", x_meet, baseline, COCOA)
    initial_width = BASE_FONT.getlength("우리 곧 만나요") / SCALE
    initial_left = (W - initial_width) / 2
    text("우리", initial_left, baseline, COCOA, (1 - remove) ** 2, BASE_FONT)
    text("요", initial_left + initial_width - BASE_FONT.getlength("요") / SCALE,
         baseline, COCOA, (1 - remove) ** 2, BASE_FONT)
    return canvas.resize((W, H), Image.Resampling.LANCZOS)


def main():
    # Runtime uses these exact transparent glyph masks rather than video playback.
    import json
    for name, word in [("IntroOur", "우리"), ("IntroSoon", "곧"), ("IntroBear", "곰"),
                       ("IntroMeet", "만나"), ("IntroYo", "요")]:
        mask = Image.new("L", (800, 400))
        ImageDraw.Draw(mask).text((0, 250), word, font=BASE_FONT, fill=255, anchor="ls")
        box = mask.getbbox()
        glyph = Image.new("RGBA", (box[2] - box[0], box[3] - box[1]), "white")
        glyph.putalpha(mask.crop(box))
        asset = ROOT / "GomManna/Resources/Assets.xcassets" / f"{name}.imageset"
        asset.mkdir(parents=True, exist_ok=True)
        glyph.save(asset / f"{name}.png")
        (asset / "Contents.json").write_text(json.dumps({
            "images": [{"idiom": "universal", "filename": f"{name}.png"}],
            "info": {"author": "xcode", "version": 1}
        }, indent=2) + "\n")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gommanna-intro-") as temporary:
        temp = Path(temporary)
        for theme in THEMES:
            (temp / theme).mkdir()
        for frame in range(round(DURATION * FPS)):
            transparent = render(frame / FPS)
            transparent.save(temp / f"{frame:04d}.png")
            for theme, colors in THEMES.items():
                lettering = Image.new("RGBA", (W, H), (*colors["text"], 255))
                lettering.putalpha(transparent.getchannel("A"))
                themed = lettering
                themed.save(temp / theme / f"{frame:04d}.png")
                if frame == 0:
                    themed.save(OUTPUT / f"gommanna-intro-{theme}-poster.png")
        for theme in THEMES:
            video = OUTPUT / f"gommanna-intro-{theme}.mov"
            subprocess.run([
                "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                "-framerate", str(FPS), "-i", str(temp / theme / "%04d.png"),
                "-an", "-c:v", "hevc_videotoolbox", "-alpha_quality", "1",
                "-pix_fmt", "bgra", "-tag:v", "hvc1", str(video),
            ], check=True)
            (OUTPUT / f"gommanna-intro-{theme}.mp4").unlink(missing_ok=True)
        subprocess.run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
            "-framerate", str(FPS), "-i", str(temp / "%04d.png"),
            "-an", "-c:v", "prores_ks", "-profile:v", "4",
            "-pix_fmt", "yuva444p10le", "-alpha_bits", "16",
            str(OUTPUT / "gommanna-intro.mov"),
        ], check=True)
        subprocess.run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
            "-framerate", str(FPS), "-i", str(temp / "%04d.png"),
            "-an", "-c:v", "libvpx-vp9", "-pix_fmt", "yuva420p",
            "-b:v", "0", "-crf", "24", "-auto-alt-ref", "0",
            str(OUTPUT / "gommanna-intro.webm"),
        ], check=True)
        # GIF has binary transparency. Ordered alpha dithering approximates
        # partial opacity without adding an opaque background to faded letters.
        import numpy as np
        bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                          [3, 11, 1, 9], [15, 7, 13, 5]], dtype=float)
        threshold = np.tile((bayer + 0.5) * 16, (135, 203))[:540, :810]
        frames = []
        for frame in range(round(DURATION * 25)):
            image = render(frame / 25).resize((810, 540), Image.Resampling.LANCZOS)
            visible = np.asarray(image.getchannel("A")) > threshold
            indexed = Image.fromarray(visible.astype("uint8"))
            indexed.putpalette([0, 0, 0, *COCOA] + [0] * (768 - 6))
            frames.append(indexed)
        for name, color in [("gommanna-intro", COCOA)] + [
            (f"gommanna-intro-{theme}", colors["text"]) for theme, colors in THEMES.items()
        ]:
            for image in frames:
                image.putpalette([0, 0, 0, *color] + [0] * (768 - 6))
            frames[0].save(OUTPUT / f"{name}.gif", save_all=True,
                           append_images=frames[1:], duration=40, loop=0,
                           transparency=0, disposal=2, optimize=False)
    times = [INITIAL_HOLD + t / PLAYBACK_SPEED for t in [0, 1.0, 1.65, 2.6]]
    board = Image.new("RGBA", (1080, 720))
    for i, timestamp in enumerate(times):
        board.alpha_composite(render(timestamp).resize((540, 360), Image.Resampling.LANCZOS),
                              ((i % 2) * 540, (i // 2) * 360))
    board.save(OUTPUT / "gommanna-intro-storyboard.png")
    # Remove superseded exports that cannot preserve transparency.
    for filename in ["gommanna-intro.mp4", "gommanna-intro-storyboard.jpg"]:
        (OUTPUT / filename).unlink(missing_ok=True)
    print(f"Rendered {DURATION}s transparent MOV, WebM and GIF in {OUTPUT}")


if __name__ == "__main__":
    main()
