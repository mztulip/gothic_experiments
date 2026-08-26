import numpy as np
from PIL import Image, ImageFilter, ImageDraw, ImageChops
import os, math

OUT = "out"
PREV = "preview"
os.makedirs(OUT, exist_ok=True)
os.makedirs(PREV, exist_ok=True)


# ---------------------------------------------------------------
# Noise helpers
# ---------------------------------------------------------------
def value_noise(w, h, octaves=5, persistence=0.55, seed=0, base_cell=6):
    r = np.random.default_rng(seed)
    result = np.zeros((h, w), dtype=np.float32)
    amp = 1.0
    total_amp = 0.0
    for o in range(octaves):
        cell = max(2, int(base_cell * (2 ** o)))
        sw = max(2, w // cell + 2)
        sh = max(2, h // cell + 2)
        noise = r.random((sh, sw)).astype(np.float32)
        img = Image.fromarray((noise * 255).astype(np.uint8), mode="L").resize((w, h), Image.BICUBIC)
        arr = np.array(img).astype(np.float32) / 255.0
        result += arr * amp
        total_amp += amp
        amp *= persistence
    result /= total_amp
    result -= result.min()
    result /= (result.max() + 1e-6)
    return result  # 0..1, shape (h,w)


def normalize(a):
    a = a - a.min()
    m = a.max()
    return a / m if m > 1e-6 else a


def colorize(noise, stops):
    """stops: list of (t, (r,g,b)) sorted by t in [0,1]"""
    h, w = noise.shape
    out = np.zeros((h, w, 3), dtype=np.float32)
    for i in range(len(stops) - 1):
        t0, c0 = stops[i]
        t1, c1 = stops[i + 1]
        mask = (noise >= t0) & (noise <= t1)
        if not mask.any():
            continue
        local_t = np.clip((noise[mask] - t0) / max(1e-6, (t1 - t0)), 0, 1)
        for ch in range(3):
            out[..., ch][mask] = c0[ch] + (c1[ch] - c0[ch]) * local_t
    return out


def add_vignette_noise(arr, seed, strength=8):
    r = np.random.default_rng(seed)
    h, w = arr.shape[:2]
    fine = r.normal(0, strength, (h, w, 1))
    return np.clip(arr + fine, 0, 255)


def to_img(arr):
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))


def save_tga(img, name):
    path = os.path.join(OUT, name)
    img.save(path, format="TGA")
    prev = img.convert("RGB") if img.mode == "RGBA" else img
    prev.save(os.path.join(PREV, name.replace(".TGA", ".png").replace(".tga", ".png")))
    print("saved", path, img.size, img.mode)


# ---------------------------------------------------------------
# 1) NW_MISC_FULLALPHA_01.TGA  16x16 - fully transparent helper texture
# ---------------------------------------------------------------
def gen_fullalpha():
    img = Image.new("RGBA", (16, 16), (128, 0, 128, 0))  # magenta base, alpha 0
    save_tga(img, "NW_Misc_FullAlpha_01.TGA")


# ---------------------------------------------------------------
# 2) NW_WATER_LAKE01_A0.TGA 256x256 - lake water, single anim frame
# ---------------------------------------------------------------
def gen_water():
    w = h = 256
    n1 = value_noise(w, h, octaves=5, persistence=0.55, seed=11, base_cell=14)
    n2 = value_noise(w, h, octaves=6, persistence=0.45, seed=27, base_cell=5)
    # gentle directional ripple, low amplitude so it stays subtle
    x = np.linspace(0, 6 * math.pi, w)
    y = np.linspace(0, 5 * math.pi, h)
    xx, yy = np.meshgrid(x, y)
    waves = 0.5 + 0.5 * np.sin(xx * 0.6 + n1 * 2.0) * np.cos(yy * 0.5 + n2 * 1.5)
    ripple = normalize(n1 * 0.5 + n2 * 0.2 + waves * 0.3)
    # push most values into the mid range so it reads as calm water, not noise storm
    ripple = 0.5 + (ripple - 0.5) * 0.55

    stops = [
        (0.0, (14, 46, 78)),
        (0.35, (20, 72, 108)),
        (0.55, (28, 96, 128)),
        (0.72, (48, 128, 150)),
        (0.86, (90, 165, 175)),
        (1.0, (150, 205, 200)),
    ]
    rgb = colorize(ripple, stops)
    rgb = add_vignette_noise(rgb, seed=5, strength=3)
    img = to_img(rgb).convert("RGB")
    img = img.filter(ImageFilter.GaussianBlur(0.8))
    # a few soft highlight glints instead of harsh veins
    highlight = normalize(n2) 
    glint_mask = np.clip((highlight - 0.78) * 6, 0, 1) * 255
    glint_img = Image.fromarray(glint_mask.astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    img = Image.composite(Image.new("RGB", (w, h), (215, 235, 232)), img, glint_img)
    save_tga(img, "NW_Water_Lake01_A0.TGA")


# ---------------------------------------------------------------
# 3) NW_SEQ_NATURE_WHITESAND_01.TGA 512x512 - white sand
# ---------------------------------------------------------------
def gen_whitesand():
    w = h = 512
    n = value_noise(w, h, octaves=6, persistence=0.5, seed=41, base_cell=5)
    fine = value_noise(w, h, octaves=3, persistence=0.6, seed=42, base_cell=2)
    mix = normalize(n * 0.7 + fine * 0.3)
    stops = [
        (0.0, (196, 178, 148)),
        (0.35, (214, 198, 168)),
        (0.6, (228, 214, 184)),
        (0.85, (240, 230, 205)),
        (1.0, (250, 244, 224)),
    ]
    rgb = colorize(mix, stops)
    rgb = add_vignette_noise(rgb, seed=43, strength=6)
    img = to_img(rgb)
    # subtle ripple lines
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(44)
    for i in range(18):
        y0 = r.integers(0, h)
        amp = r.integers(4, 14)
        pts = []
        for x in range(0, w, 8):
            yy = y0 + amp * math.sin(x * 0.03 + i) 
            pts.append((x, yy))
        draw.line(pts, fill=(170, 150, 120, 25), width=2)
    save_tga(img.filter(ImageFilter.SMOOTH), "NW_Seq_Nature_WhiteSand_01.TGA")


# ---------------------------------------------------------------
# 4/5) Ship wood textures  MoWoShip02 (256x256) / MoWoShip01 (64x128)
# ---------------------------------------------------------------
def gen_wood_planks(w, h, seed, plank_h):
    n = value_noise(w, h, octaves=5, persistence=0.55, seed=seed, base_cell=3)
    grain = value_noise(w, h, octaves=6, persistence=0.45, seed=seed + 1, base_cell=1)
    # stretch grain horizontally to emulate wood fibre
    grain_img = Image.fromarray((grain * 255).astype(np.uint8))
    grain_img = grain_img.resize((max(1, w // 6), h)).resize((w, h), Image.BICUBIC)
    grain = np.array(grain_img).astype(np.float32) / 255.0
    mix = normalize(n * 0.4 + grain * 0.6)
    stops = [
        (0.0, (60, 38, 22)),
        (0.3, (90, 58, 32)),
        (0.55, (118, 80, 46)),
        (0.8, (140, 100, 60)),
        (1.0, (168, 128, 82)),
    ]
    rgb = colorize(mix, stops)
    img = to_img(add_vignette_noise(rgb, seed + 2, strength=6))
    draw = ImageDraw.Draw(img, "RGBA")
    # plank seams (horizontal)
    y = 0
    r = np.random.default_rng(seed + 3)
    while y < h:
        draw.line([(0, y), (w, y)], fill=(25, 15, 8, 200), width=1)
        y += plank_h + r.integers(-2, 3)
    # nails
    for yy in range(plank_h // 2, h, plank_h):
        for xx in range(8, w, 24):
            draw.ellipse([xx - 1, yy - 1, xx + 1, yy + 1], fill=(30, 20, 12, 180))
    return img.filter(ImageFilter.SMOOTH)


def gen_ship_wood():
    save_tga(gen_wood_planks(256, 256, seed=61, plank_h=34), "MoWoShip02.TGA")
    save_tga(gen_wood_planks(64, 128, seed=71, plank_h=22), "MoWoShip01.TGA")


# ---------------------------------------------------------------
# 6/7) Cave wall rock textures 512x512 (two variants)
# ---------------------------------------------------------------
def gen_cave_wall(seed, name):
    w = h = 512
    n1 = value_noise(w, h, octaves=6, persistence=0.55, seed=seed, base_cell=8)
    n2 = value_noise(w, h, octaves=7, persistence=0.4, seed=seed + 1, base_cell=2)
    mix = normalize(n1 * 0.55 + n2 * 0.45)
    stops = [
        (0.0, (28, 24, 20)),
        (0.25, (48, 42, 34)),
        (0.5, (70, 62, 50)),
        (0.72, (95, 84, 66)),
        (0.9, (120, 108, 86)),
        (1.0, (145, 132, 108)),
    ]
    rgb = colorize(mix, stops)
    rgb = add_vignette_noise(rgb, seed + 2, strength=10)
    img = to_img(rgb)
    # crack lines
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(seed + 5)
    for i in range(14):
        x, y = r.integers(0, w), r.integers(0, h)
        pts = [(x, y)]
        ang = r.random() * 2 * math.pi
        for s in range(r.integers(6, 14)):
            ang += r.normal(0, 0.5)
            x = (x + math.cos(ang) * r.integers(10, 24))
            y = (y + math.sin(ang) * r.integers(10, 24))
            pts.append((x, y))
        draw.line(pts, fill=(15, 12, 10, 140), width=r.integers(1, 3))
    save_tga(img.filter(ImageFilter.SMOOTH), name)


def gen_cave_walls():
    gen_cave_wall(81, "NW_Misc_CaveWall_01.TGA")
    gen_cave_wall(91, "NW_Misc_CaveWall_02.TGA")


# ---------------------------------------------------------------
# 8) PALME.TGA - missing texture, palm frond leaf (with alpha)
# ---------------------------------------------------------------
def gen_palme():
    w, h = 512, 512
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(101)
    cx, cy = w // 2, h - 10
    n_fronds = 9
    for i in range(n_fronds):
        base_ang = -math.pi / 2 + (i - n_fronds / 2) * (math.pi / (n_fronds - 1)) * 1.15
        length = r.integers(210, 260)
        segs = 26
        left_pts, right_pts = [], []
        gx, gy = cx, cy
        ang = base_ang
        for s in range(segs):
            t = s / segs
            width_here = max(1.5, 16 * (1 - t) * math.sin(t * math.pi + 0.2))
            gx = cx + math.cos(ang) * length * t
            gy = cy + math.sin(ang) * length * t - 40 * t * t
            perp = ang + math.pi / 2
            left_pts.append((gx + math.cos(perp) * width_here, gy + math.sin(perp) * width_here))
            right_pts.append((gx - math.cos(perp) * width_here, gy - math.sin(perp) * width_here))
        poly = left_pts + right_pts[::-1]
        green = (
            int(40 + r.integers(0, 25)),
            int(90 + r.integers(0, 40)),
            int(30 + r.integers(0, 15)),
            255,
        )
        draw.polygon(poly, fill=green)
        # midrib line
        draw.line([(cx, cy)] + [((left_pts[k][0] + right_pts[k][0]) / 2,
                                  (left_pts[k][1] + right_pts[k][1]) / 2) for k in range(segs)],
                   fill=(20, 60, 15, 200), width=2)
    # trunk stub at bottom
    draw.polygon([(cx - 14, cy), (cx + 14, cy), (cx + 10, h - 1), (cx - 10, h - 1)],
                 fill=(90, 65, 40, 255))
    img = img.filter(ImageFilter.SMOOTH)
    save_tga(img, "PALME.TGA")


# ---------------------------------------------------------------
# 9) NW_SEQ_NATURE_PALMBARK_01.TGA 256x2048 - tall palm trunk bark
# ---------------------------------------------------------------
def gen_palmbark():
    w, h = 256, 2048
    n = value_noise(w, h, octaves=6, persistence=0.5, seed=121, base_cell=6)
    stops = [
        (0.0, (58, 40, 24)),
        (0.3, (84, 60, 36)),
        (0.55, (110, 82, 50)),
        (0.8, (135, 104, 66)),
        (1.0, (158, 126, 84)),
    ]
    rgb = colorize(n, stops)
    img = to_img(add_vignette_noise(rgb, 122, strength=8))
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(123)
    # horizontal ring scars typical of palm trunks
    y = 0
    while y < h:
        wob = r.integers(-6, 6)
        pts = []
        for x in range(0, w, 6):
            yy = y + wob * math.sin(x * 0.05)
            pts.append((x, yy))
        draw.line(pts, fill=(35, 22, 12, 210), width=r.integers(2, 5))
        y += r.integers(50, 90)
    # vertical fibre streaks
    for i in range(90):
        x = r.integers(0, w)
        draw.line([(x, 0), (x + r.integers(-10, 10), h)], fill=(90, 65, 40, 25), width=1)
    save_tga(img.filter(ImageFilter.SMOOTH), "NW_Seq_Nature_PalmBark_01.TGA")


# ---------------------------------------------------------------
# 10) OW_ORCS_TENTWALL_01.TGA 1024x512  &  03 (256x128) - hide/canvas tent wall
# ---------------------------------------------------------------
def gen_tentwall(w, h, seed, name, stitch_spacing=64):
    n = value_noise(w, h, octaves=6, persistence=0.5, seed=seed, base_cell=6)
    fine = value_noise(w, h, octaves=4, persistence=0.4, seed=seed + 1, base_cell=2)
    mix = normalize(n * 0.6 + fine * 0.4)
    stops = [
        (0.0, (54, 40, 26)),
        (0.3, (78, 58, 38)),
        (0.55, (100, 76, 50)),
        (0.8, (122, 96, 64)),
        (1.0, (144, 116, 80)),
    ]
    rgb = colorize(mix, stops)
    img = to_img(add_vignette_noise(rgb, seed + 2, strength=9))
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(seed + 3)
    # hide patch seams
    x = 0
    while x < w:
        draw.line([(x, 0), (x, h)], fill=(20, 14, 8, 160), width=2)
        x += stitch_spacing + r.integers(-8, 8)
    y = 0
    while y < h:
        draw.line([(0, y), (w, y)], fill=(20, 14, 8, 130), width=2)
        y += stitch_spacing + r.integers(-8, 8)
    # stitch dashes along a few seams
    for i in range(0, w, stitch_spacing):
        for yy in range(0, h, 8):
            if r.random() < 0.5:
                draw.line([(i - 3, yy), (i + 3, yy)], fill=(200, 180, 140, 90), width=1)
    save_tga(img.filter(ImageFilter.SMOOTH), name)


def gen_tentwalls():
    gen_tentwall(1024, 512, 141, "OW_Orcs_TentWall_01.TGA", stitch_spacing=96)
    gen_tentwall(256, 128, 151, "OW_Orcs_TentWall_03.TGA", stitch_spacing=48)


# ---------------------------------------------------------------
# 11) NW_CITY_ROPE_01.TGA 128x1024 - twisted rope, vertical tiling
# ---------------------------------------------------------------
def gen_rope():
    w, h = 128, 1024
    base = np.zeros((h, w, 3), dtype=np.float32)
    cx = w / 2
    strand_r = w * 0.30
    n = value_noise(w, h, octaves=4, persistence=0.5, seed=161, base_cell=3)
    for yy in range(h):
        for _ in range(0):
            pass
    yy_arr = np.arange(h)
    twist = yy_arr * (2 * math.pi / 40.0)
    img = Image.new("RGB", (w, h), (70, 55, 30))
    draw = ImageDraw.Draw(img)
    strands = 3
    for y in range(h):
        t = twist[y]
        for s in range(strands):
            ang = t + s * (2 * math.pi / strands)
            sx = cx + math.cos(ang) * strand_r
            shade = 0.55 + 0.45 * math.sin(ang)
            col = (
                int(120 * shade + 40),
                int(95 * shade + 30),
                int(55 * shade + 15),
            )
            rr = 10 + 3 * math.sin(ang * 2)
            draw.ellipse([sx - rr, y - 1, sx + rr, y + 1], fill=col)
    arr = np.array(img).astype(np.float32)
    noise_rgb = np.stack([n, n, n], axis=-1) * 18 - 9
    arr = np.clip(arr + noise_rgb, 0, 255)
    out_img = to_img(arr).filter(ImageFilter.SMOOTH)
    save_tga(out_img, "NW_City_Rope_01.TGA")


# ---------------------------------------------------------------
# 12) NW_NATURE_BARK_07.TGA 128x512 - tree bark, vertical
# ---------------------------------------------------------------
def gen_treebark():
    w, h = 128, 512
    n = value_noise(w, h, octaves=6, persistence=0.55, seed=181, base_cell=4)
    stretched = Image.fromarray((n * 255).astype(np.uint8)).resize((w, h // 4)).resize((w, h), Image.BICUBIC)
    n2 = np.array(stretched).astype(np.float32) / 255.0
    mix = normalize(n * 0.45 + n2 * 0.55)
    stops = [
        (0.0, (34, 24, 16)),
        (0.3, (56, 40, 26)),
        (0.55, (78, 58, 38)),
        (0.8, (98, 76, 50)),
        (1.0, (120, 96, 64)),
    ]
    rgb = colorize(mix, stops)
    img = to_img(add_vignette_noise(rgb, 182, strength=9))
    draw = ImageDraw.Draw(img, "RGBA")
    r = np.random.default_rng(183)
    for i in range(60):
        x = r.integers(0, w)
        pts = []
        yy = 0
        xx = x
        while yy < h:
            pts.append((xx, yy))
            xx += r.integers(-3, 4)
            yy += r.integers(6, 14)
        draw.line(pts, fill=(20, 14, 9, 120), width=1)
    save_tga(img.filter(ImageFilter.SMOOTH), "NW_Nature_Bark_07.TGA")


if __name__ == "__main__":
    gen_fullalpha()
    gen_water()
    gen_whitesand()
    gen_ship_wood()
    gen_cave_walls()
    gen_palme()
    gen_palmbark()
    gen_tentwalls()
    gen_rope()
    gen_treebark()
    print("DONE")
