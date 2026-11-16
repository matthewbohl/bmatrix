#!/usr/bin/env python3
"""Create 15 deterministic, redistributable PPM images for bmatrix visual QA."""
import argparse
import math
from pathlib import Path


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def clamp(value):
    return max(0, min(255, round(value)))


def write(name, width, height, pixel):
    payload = bytearray()
    for y in range(height):
        for x in range(width):
            payload.extend(clamp(channel) for channel in pixel(x, y, width, height))
    (args.output / name).write_bytes(f"P6\n{width} {height}\n255\n".encode() + payload)


def gradient(x, y, w, h):
    return (255 * x / (w - 1), 255 * y / (h - 1), 255 * (1 - x / (w - 1)))


def sunset(x, y, w, h):
    horizon = h * .58
    if y < horizon:
        t = y / horizon
        return (255, 80 + 120 * t, 40 + 150 * t)
    return (12 + 35 * math.sin(x / 9) ** 2, 18, 38)


def orbit(x, y, w, h):
    dx, dy = x - w / 2, y - h / 2
    distance = math.hypot(dx, dy)
    ring = max(0, 1 - abs(distance - min(w, h) * .28) / 10)
    return (20 + 210 * ring, 35 + 80 * ring, 75 + 160 * ring)


def checker(x, y, w, h):
    value = 235 if (x // 16 + y // 16) % 2 else 25
    return (value, value, value)


def stripes(x, y, w, h):
    return (35 + 190 * (x // 9 % 2), 30 + 175 * (y // 13 % 2), 70 + 140 * ((x + y) // 17 % 2))


def portrait(x, y, w, h):
    background = (28 + x * 25 / w, 55 + y * 50 / h, 85 + x * 40 / w)
    dx, dy = (x - w / 2) / (w * .22), (y - h * .38) / (h * .20)
    if dx * dx + dy * dy < 1:
        return (238, 188, 142)
    if h * .50 < y < h * .94 and abs(x - w / 2) < w * .32:
        return (45, 80, 145)
    return background


def city(x, y, w, h):
    horizon = h * .68
    if y < horizon:
        return (18 + y * 35 / h, 25 + y * 45 / h, 60 + y * 70 / h)
    building = ((x // 17) * 29) % int(h * .32) + h * .42
    if y > building:
        window = (x // 5 + y // 7) % 5 == 0
        return (205 if window else 12, 155 if window else 18, 65 if window else 30)
    return (18, 25, 52)


def waves(x, y, w, h):
    value = .5 + .5 * math.sin(x / 11 + math.sin(y / 15) * 2)
    return (25 + 30 * value, 60 + 145 * value, 100 + 135 * value)


def edges(x, y, w, h):
    block = ((x // 24) + (y // 24)) % 2
    line = x % 24 < 2 or y % 24 < 2
    return (250 if line else 20 + 120 * block, 250 if line else 25, 250 if line else 90 - 55 * block)


def noise(x, y, w, h):
    value = ((x * 1103515245 + y * 12345) >> 9) & 255
    return (value, 255 - value, (value * 3) & 255)


def vignette(x, y, w, h):
    dx, dy = (x - w / 2) / w, (y - h / 2) / h
    value = max(0, 1 - math.hypot(dx, dy) * 1.8)
    return (255 * value, 205 * value, 90 * value)


fixtures = [
    ("01-wide-gradient.ppm", 360, 160, gradient),
    ("02-tall-gradient.ppm", 160, 360, gradient),
    ("03-square-orbit.ppm", 256, 256, orbit),
    ("04-high-contrast-checker.ppm", 320, 180, checker),
    ("05-color-stripes.ppm", 320, 180, stripes),
    ("06-portrait-subject.ppm", 180, 360, portrait),
    ("07-wide-sunset.ppm", 400, 160, sunset),
    ("08-city-silhouette.ppm", 360, 200, city),
    ("09-ocean-waves.ppm", 360, 200, waves),
    ("10-edge-grid.ppm", 320, 240, edges),
    ("11-color-noise.ppm", 320, 180, noise),
    ("12-dark-vignette.ppm", 320, 180, vignette),
    ("13-ultrawide-gradient.ppm", 480, 120, gradient),
    ("14-ultratall-portrait.ppm", 120, 480, portrait),
    ("15-small-square.ppm", 96, 96, orbit),
]

for fixture in fixtures:
    write(*fixture)
print(args.output.resolve())
