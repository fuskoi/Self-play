#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 STL 生成装配/零件预览 PNG（numpy + Pillow，画家算法）"""

from __future__ import annotations

import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

STL_DIR = Path(__file__).resolve().parent / "stl"
OUT_DIR = Path(__file__).resolve().parent / "preview"


def load_stl(path: Path) -> np.ndarray:
    raw = path.read_bytes()
    n = struct.unpack_from("<I", raw, 80)[0]
    tris = np.empty((n, 3, 3), dtype=np.float32)
    off = 84
    for i in range(n):
        verts = struct.unpack_from("<9f", raw, off + 12)
        tris[i] = np.array(verts, dtype=np.float32).reshape(3, 3)
        off += 50
    return tris


def transform(tris: np.ndarray, dx=0.0, dy=0.0, dz=0.0) -> np.ndarray:
    out = tris.copy()
    out[..., 0] += dx
    out[..., 1] += dy
    out[..., 2] += dz
    return out


def project_iso(pts: np.ndarray) -> np.ndarray:
    ax = np.deg2rad(-58)
    az = np.deg2rad(38)
    cx, sx = np.cos(ax), np.sin(ax)
    cz, sz = np.cos(az), np.sin(az)
    x, y, z = pts[..., 0], pts[..., 1], pts[..., 2]
    x1 = x * cz - y * sz
    y1 = x * sz + y * cz
    y2 = y1 * cx - z * sx
    z2 = y1 * sx + z * cx
    return np.stack([x1, z2, -y2], axis=-1)


def subsample(tris: np.ndarray, max_tris: int) -> np.ndarray:
    if len(tris) <= max_tris:
        return tris
    # 均匀采样保留外形
    idx = np.linspace(0, len(tris) - 1, max_tris).astype(np.int64)
    return tris[idx]


def render(meshes: list[tuple[np.ndarray, tuple[int, int, int]]], size=900, title="") -> Image.Image:
    chunks = []
    colors = []
    for tris, color in meshes:
        chunks.append(tris)
        colors.append(np.broadcast_to(np.array(color, dtype=np.float32), (len(tris), 3)).copy())
    all_tris = np.concatenate(chunks, axis=0)
    all_col = np.concatenate(colors, axis=0)

    proj = project_iso(all_tris)  # N,3,3
    # depth = centroid z
    depth = proj[:, :, 2].mean(axis=1)
    order = np.argsort(depth)  # far → near

    umin, vmin = proj[:, :, 0].min(), proj[:, :, 1].min()
    umax, vmax = proj[:, :, 0].max(), proj[:, :, 1].max()
    span = max(umax - umin, vmax - vmin) or 1.0
    pad = span * 0.1
    umin -= pad
    vmin -= pad
    span = max(umax - umin + pad, vmax - vmin + pad)

    # lighting from original normals
    v0, v1, v2 = all_tris[:, 0], all_tris[:, 1], all_tris[:, 2]
    n = np.cross(v1 - v0, v2 - v0)
    nn = np.linalg.norm(n, axis=1, keepdims=True)
    nn[nn < 1e-12] = 1.0
    n = n / nn
    light = np.array([0.4, 0.7, 0.55], dtype=np.float32)
    light /= np.linalg.norm(light)
    shade = np.clip(n @ light, 0.22, 1.0)

    img = Image.new("RGB", (size, size), (246, 247, 250))
    draw = ImageDraw.Draw(img, "RGBA")

    u = proj[:, :, 0]
    v = proj[:, :, 1]
    sx = (u - umin) / span * (size - 1)
    sy = (1.0 - (v - vmin) / span) * (size - 1)

    for i in order:
        pts = [
            (float(sx[i, 0]), float(sy[i, 0])),
            (float(sx[i, 1]), float(sy[i, 1])),
            (float(sx[i, 2]), float(sy[i, 2])),
        ]
        s = float(shade[i])
        r, g, b = all_col[i]
        fill = (int(r * s), int(g * s), int(b * s), 255)
        draw.polygon(pts, fill=fill)

    if title:
        d2 = ImageDraw.Draw(img)
        d2.rectangle((0, 0, size, 44), fill=(246, 247, 250))
        d2.text((20, 14), title, fill=(40, 45, 55))
    return img


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    base = subsample(load_stl(STL_DIR / "01_ceiling_base.stl"), 12000)
    neck = subsample(load_stl(STL_DIR / "02_neck.stl"), 2000)
    upper = subsample(load_stl(STL_DIR / "03_shell_upper.stl"), 18000)
    lower = subsample(load_stl(STL_DIR / "04_shell_lower.stl"), 18000)

    white = (232, 234, 238)
    accent = (110, 165, 210)
    dark = (95, 115, 135)
    soft = (205, 210, 218)

    assembly = [
        (transform(lower, 0, 0, 0), white),
        (transform(upper, 0, 0, 22), soft),
        (transform(neck, 0, 0, 55), accent),
        (transform(base, 0, 0, 86), dark),
    ]
    img = render(assembly, size=1000, title="Assembly  ·  ceiling base + neck + upper + lower")
    p = OUT_DIR / "00_assembly.png"
    img.save(p, optimize=True)
    print("wrote", p)

    parts = [
        ("01_ceiling_base.png", base, dark, "01  ceiling base"),
        ("02_neck.png", neck, accent, "02  neck tube"),
        ("03_shell_upper.png", upper, soft, "03  shell upper"),
        ("04_shell_lower.png", lower, white, "04  shell lower"),
    ]
    thumbs = []
    for name, mesh, color, title in parts:
        im = render([(mesh, color)], size=640, title=title)
        path = OUT_DIR / name
        im.save(path, optimize=True)
        print("wrote", path)
        thumbs.append(im)

    grid = Image.new("RGB", (1280, 1280), (246, 247, 250))
    for im, pos in zip(thumbs, [(0, 0), (640, 0), (0, 640), (640, 640)]):
        grid.paste(im, pos)
    gpath = OUT_DIR / "00_parts_grid.png"
    grid.save(gpath, optimize=True)
    print("wrote", gpath)


if __name__ == "__main__":
    main()
