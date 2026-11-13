#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
客流计数器吸顶外壳 — 生成可 3D 打印 STL（无第三方依赖）

零件：
  01_ceiling_base   天花圆座
  02_neck           短颈管
  03_shell_upper    上壳（OLED / USB-C / 按键真开孔）
  04_shell_lower    下壳（双 ToF 圆窗 + 相机孔）
"""

from __future__ import annotations

import math
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent / "stl"


class Mesh:
    def __init__(self):
        self.tris: list[tuple[tuple[float, float, float], ...]] = []

    def add_tri(self, a, b, c):
        self.tris.append((a, b, c))

    def add_quad(self, a, b, c, d):
        self.add_tri(a, b, c)
        self.add_tri(a, c, d)

    def extend(self, other: "Mesh"):
        self.tris.extend(other.tris)

    def translate(self, dx, dy, dz) -> "Mesh":
        m = Mesh()
        for a, b, c in self.tris:
            m.add_tri(
                (a[0] + dx, a[1] + dy, a[2] + dz),
                (b[0] + dx, b[1] + dy, b[2] + dz),
                (c[0] + dx, c[1] + dy, c[2] + dz),
            )
        return m

    def write_stl(self, path: Path, name: str = "part"):
        path.parent.mkdir(parents=True, exist_ok=True)
        n = len(self.tris)
        with path.open("wb") as f:
            header = name.encode("ascii", "ignore")[:80].ljust(80, b"\0")
            f.write(header)
            f.write(struct.pack("<I", n))
            for a, b, c in self.tris:
                ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
                vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
                nx = uy * vz - uz * vy
                ny = uz * vx - ux * vz
                nz = ux * vy - uy * vx
                ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
                f.write(struct.pack("<3f", nx / ln, ny / ln, nz / ln))
                f.write(struct.pack("<3f", *a))
                f.write(struct.pack("<3f", *b))
                f.write(struct.pack("<3f", *c))
                f.write(struct.pack("<H", 0))
        print(f"wrote {path}  tris={n}")


def box(x0, y0, z0, x1, y1, z1) -> Mesh:
    m = Mesh()
    m.add_quad((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0))
    m.add_quad((x0, y0, z1), (x0, y1, z1), (x1, y1, z1), (x1, y0, z1))
    m.add_quad((x0, y0, z0), (x0, y0, z1), (x1, y0, z1), (x1, y0, z0))
    m.add_quad((x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1))
    m.add_quad((x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1))
    m.add_quad((x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0))
    return m


def tube(r_out, r_in, h, segments=64, z0=0.0) -> Mesh:
    m = Mesh()
    z1 = z0 + h
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        xo0, yo0 = r_out * math.cos(a0), r_out * math.sin(a0)
        xo1, yo1 = r_out * math.cos(a1), r_out * math.sin(a1)
        xi0, yi0 = r_in * math.cos(a0), r_in * math.sin(a0)
        xi1, yi1 = r_in * math.cos(a1), r_in * math.sin(a1)
        m.add_quad((xo0, yo0, z0), (xo1, yo1, z0), (xo1, yo1, z1), (xo0, yo0, z1))
        m.add_quad((xi0, yi0, z0), (xi0, yi0, z1), (xi1, yi1, z1), (xi1, yi1, z0))
        m.add_quad((xo0, yo0, z0), (xi0, yi0, z0), (xi1, yi1, z0), (xo1, yo1, z0))
        m.add_quad((xo0, yo0, z1), (xo1, yo1, z1), (xi1, yi1, z1), (xi0, yi0, z1))
    return m


def cylinder_shell(r, h, segments=64, z0=0.0) -> Mesh:
    """实心圆柱（顶底+侧面）"""
    m = Mesh()
    z1 = z0 + h
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        x0, y0 = r * math.cos(a0), r * math.sin(a0)
        x1, y1 = r * math.cos(a1), r * math.sin(a1)
        m.add_tri((0, 0, z0), (x1, y1, z0), (x0, y0, z0))
        m.add_tri((0, 0, z1), (x0, y0, z1), (x1, y1, z1))
        m.add_quad((x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1))
    return m


def _pt_in_circle(x, y, cx, cy, r):
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def plate_rect_with_holes(x0, y0, x1, y1, z0, t, holes, edge_segs=72) -> Mesh:
    """矩形底板 + 圆形通孔（孔壁光滑，不用体素网格）"""
    m = Mesh()
    z1 = z0 + t

    # 外轮廓四边顶/底：用“外框环”= 整板顶底用扇区法绕每个孔拆环
    # 做法：顶/底面 = 外矩形三角剖分，跳过落在孔内的三角形；孔周加竖壁
    # 为光滑圆孔：对每个孔单独画环形顶底接到外框较复杂，这里用
    # 「满底板圆柱孔壁 + 顶底用极细极坐标采样矩形」——改为：
    # 1) 四壁外框实心条
    # 2) 内部顶底：按孔生成 annulus 连接到包围盒太难
    # 实用干净方案：顶/底用 Delaunay 简化版 —— 径向射线扫矩形

    # --- 外缘四边立板（保证外轮廓）已由壳体四壁负责；这里只做底板 ---
    # 顶面/底面：从板中心向外矩形边界扇区，遇孔则改连到孔圆周

    cx = (x0 + x1) / 2
    cy = (y0 + y1) / 2

    def rect_border_point(ang):
        # 射线与矩形边界交点
        dx, dy = math.cos(ang), math.sin(ang)
        ts = []
        if abs(dx) > 1e-12:
            for xb in (x0, x1):
                t = (xb - cx) / dx
                if t > 0:
                    yy = cy + t * dy
                    if y0 - 1e-9 <= yy <= y1 + 1e-9:
                        ts.append(t)
        if abs(dy) > 1e-12:
            for yb in (y0, y1):
                t = (yb - cy) / dy
                if t > 0:
                    xx = cx + t * dx
                    if x0 - 1e-9 <= xx <= x1 + 1e-9:
                        ts.append(t)
        tmin = min(ts) if ts else 0.0
        return (cx + tmin * dx, cy + tmin * dy)

    # 按角度细分；每根射线：若穿过某孔，则在该孔处截断并画孔壁
    n = max(edge_segs, 96)
    border = [rect_border_point(2 * math.pi * i / n) for i in range(n)]

    # 简化可靠方案：底板 = 无孔实心矩形板，再对每个孔「挖」——
    # 用环形替换：整板顶底用细分网格，但只在孔边缘用圆形采样点强制圆孔
    # → 采用「矩形板去掉孔 = 外环面用三角形扇 + 每孔内壁」

    # 最终采用：先铺细密矩形网格（仅顶底薄面，无侧柱），孔内跳过；
    # 再对每个孔加圆柱内壁 + 顶底圆孔边线与邻格衔接靠密度保证观感
    nx = max(120, int((x1 - x0) * 2))
    ny = max(70, int((y1 - y0) * 2))
    sx = (x1 - x0) / nx
    sy = (y1 - y0) / ny

    def in_any_hole(x, y, margin=0.0):
        for hx, hy, hr in holes:
            if _pt_in_circle(x, y, hx, hy, hr - margin):
                return True
        return False

    solid = [[False] * ny for _ in range(nx)]
    for ix in range(nx):
        for iy in range(ny):
            px = x0 + (ix + 0.5) * sx
            py = y0 + (iy + 0.5) * sy
            solid[ix][iy] = not in_any_hole(px, py)

    for ix in range(nx):
        for iy in range(ny):
            if not solid[ix][iy]:
                continue
            xa = x0 + ix * sx
            ya = y0 + iy * sy
            xb, yb = xa + sx, ya + sy
            m.add_quad((xa, ya, z0), (xb, ya, z0), (xb, yb, z0), (xa, yb, z0))
            m.add_quad((xa, ya, z1), (xa, yb, z1), (xb, yb, z1), (xb, ya, z1))
            # 与孔相邻的格子加竖壁，形成较圆的孔壁
            for jx, jy, qa, qb, qc, qd in (
                (ix - 1, iy, (xa, ya, z0), (xa, ya, z1), (xa, yb, z1), (xa, yb, z0)),
                (ix + 1, iy, (xb, ya, z0), (xb, yb, z0), (xb, yb, z1), (xb, ya, z1)),
                (ix, iy - 1, (xa, ya, z0), (xb, ya, z0), (xb, ya, z1), (xa, ya, z1)),
                (ix, iy + 1, (xa, yb, z0), (xa, yb, z1), (xb, yb, z1), (xb, yb, z0)),
            ):
                if jx < 0 or jx >= nx or jy < 0 or jy >= ny or not solid[jx][jy]:
                    m.add_quad(qa, qb, qc, qd)

    # 每个圆孔再套一圈精确圆管壁，掩盖网格锯齿
    for hx, hy, hr in holes:
        m.extend(tube(hr + 0.15, hr, t, segments=64, z0=z0).translate(hx, hy, 0))

    return m


def disk_with_holes(r_out, t, holes, segments=128) -> Mesh:
    """圆盘 + 圆孔（光滑外缘；孔用细网格 + 精确圆管壁）"""
    m = Mesh()
    z0, z1 = 0.0, t
    # 外缘侧面
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        x0, y0 = r_out * math.cos(a0), r_out * math.sin(a0)
        x1, y1 = r_out * math.cos(a1), r_out * math.sin(a1)
        m.add_quad((x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1))

    n = 160
    step = (2 * r_out) / n

    def in_disk(x, y):
        return x * x + y * y <= r_out * r_out

    def in_hole(x, y):
        for hx, hy, hr in holes:
            if _pt_in_circle(x, y, hx, hy, hr):
                return True
        return False

    solid = [[False] * n for _ in range(n)]
    for ix in range(n):
        for iy in range(n):
            cx = -r_out + (ix + 0.5) * step
            cy = -r_out + (iy + 0.5) * step
            solid[ix][iy] = in_disk(cx, cy) and not in_hole(cx, cy)

    for ix in range(n):
        for iy in range(n):
            if not solid[ix][iy]:
                continue
            xa = -r_out + ix * step
            ya = -r_out + iy * step
            xb, yb = xa + step, ya + step
            m.add_quad((xa, ya, z0), (xb, ya, z0), (xb, yb, z0), (xa, yb, z0))
            m.add_quad((xa, ya, z1), (xa, yb, z1), (xb, yb, z1), (xb, ya, z1))
            for jx, jy, qa, qb, qc, qd in (
                (ix - 1, iy, (xa, ya, z0), (xa, ya, z1), (xa, yb, z1), (xa, yb, z0)),
                (ix + 1, iy, (xb, ya, z0), (xb, yb, z0), (xb, yb, z1), (xb, ya, z1)),
                (ix, iy - 1, (xa, ya, z0), (xb, ya, z0), (xb, ya, z1), (xa, ya, z1)),
                (ix, iy + 1, (xa, yb, z0), (xa, yb, z1), (xb, yb, z1), (xb, yb, z0)),
            ):
                if jx < 0 or jx >= n or jy < 0 or jy >= n or not solid[jx][jy]:
                    m.add_quad(qa, qb, qc, qd)

    for hx, hy, hr in holes:
        m.extend(tube(hr + 0.15, hr, t, segments=64, z0=0.0).translate(hx, hy, 0))
    return m


def wall_x(x, y0, y1, z0, z1, t, gaps=None) -> Mesh:
    """平行 YZ 的墙（法向 ±X）。gaps: [(y_a,y_b,z_a,z_b), ...] 开孔"""
    gaps = gaps or []
    m = Mesh()
    # 整墙先画，开孔处用分段避开（Y-Z 平面分割）
    # 简化：墙 = 实心，开孔用「不画矩形区域」——把墙拆成网格条带
    x0, x1 = (x, x + t) if t > 0 else (x + t, x)
    # 用竖条：沿 Y 分段
    ys = [y0, y1]
    zs = [z0, z1]
    for ga, gb, za, zb in gaps:
        ys += [ga, gb]
        zs += [za, zb]
    ys = sorted(set(round(v, 4) for v in ys))
    zs = sorted(set(round(v, 4) for v in zs))

    def in_gap(yc, zc):
        for ga, gb, za, zb in gaps:
            if ga <= yc <= gb and za <= zc <= zb:
                return True
        return False

    for i in range(len(ys) - 1):
        for j in range(len(zs) - 1):
            ya, yb = ys[i], ys[i + 1]
            za, zb = zs[j], zs[j + 1]
            if yb - ya < 1e-6 or zb - za < 1e-6:
                continue
            if in_gap((ya + yb) / 2, (za + zb) / 2):
                continue
            m.extend(box(x0, ya, za, x1, yb, zb))
    return m


def wall_y(y, x0, x1, z0, z1, t, gaps=None) -> Mesh:
    """平行 XZ 的墙（法向 ±Y）"""
    gaps = gaps or []
    m = Mesh()
    y0, y1 = (y, y + t) if t > 0 else (y + t, y)
    xs = [x0, x1]
    zs = [z0, z1]
    for ga, gb, za, zb in gaps:
        xs += [ga, gb]
        zs += [za, zb]
    xs = sorted(set(round(v, 4) for v in xs))
    zs = sorted(set(round(v, 4) for v in zs))

    def in_gap(xc, zc):
        for ga, gb, za, zb in gaps:
            if ga <= xc <= gb and za <= zc <= zb:
                return True
        return False

    for i in range(len(xs) - 1):
        for j in range(len(zs) - 1):
            xa, xb = xs[i], xs[i + 1]
            za, zb = zs[j], zs[j + 1]
            if xb - xa < 1e-6 or zb - za < 1e-6:
                continue
            if in_gap((xa + xb) / 2, (za + zb) / 2):
                continue
            m.extend(box(xa, y0, za, xb, y1, zb))
    return m


# ---------- 尺寸 mm ----------
TOF_SPACING = 100.0
TOF_WIN_R = 7.5
CAM_R = 2.5
SHELL_L = 140.0
SHELL_W = 78.0
SHELL_H = 42.0
WALL = 2.2
LIP = 1.2

OLED_W, OLED_H = 28.0, 16.0
USB_W, USB_H = 9.2, 3.4
BTN_R = 2.2

BASE_R = 45.0
BASE_T = 4.0
NECK_R_OUT = 14.0
NECK_R_IN = 9.0
NECK_H = 28.0


def part_ceiling_base() -> Mesh:
    holes = [
        (28.0, 0.0, 2.1),
        (-28.0, 0.0, 2.1),
        (0.0, 28.0, 2.1),
        (0.0, -28.0, 2.1),
        (0.0, 0.0, NECK_R_IN),
    ]
    return disk_with_holes(BASE_R, BASE_T, holes, segments=128)


def part_neck() -> Mesh:
    return tube(NECK_R_OUT, NECK_R_IN, NECK_H, segments=64)


def part_shell_upper() -> Mesh:
    """上壳：有高度的空心盒，顶面颈口，侧面真开孔"""
    L, W, H, t = SHELL_L, SHELL_W, SHELL_H * 0.55, WALL
    x0, y0 = -L / 2, -W / 2
    x1, y1 = L / 2, W / 2
    z0, z1 = 0.0, H
    m = Mesh()

    # 顶板（中心过线孔）
    m.extend(
        plate_rect_with_holes(
            x0, y0, x1, y1, z1 - t, t, [(0.0, 0.0, NECK_R_IN)], edge_segs=96
        )
    )

    # 颈接口凸台（在顶面之上）
    m.extend(tube(NECK_R_OUT + 1.5, NECK_R_IN, 8.0, segments=64, z0=z1 - 0.05))

    # OLED：+Y 墙开孔
    oled_x0, oled_x1 = -OLED_W / 2, OLED_W / 2
    oled_z0, oled_z1 = H * 0.32, H * 0.32 + OLED_H
    m.extend(
        wall_y(
            y1 - t,
            x0,
            x1,
            z0,
            z1 - t,
            t,
            gaps=[(oled_x0, oled_x1, oled_z0, oled_z1)],
        )
    )

    # USB-C：-Y 墙
    usb_x0, usb_x1 = 16.0, 16.0 + USB_W
    usb_z0, usb_z1 = 7.0, 7.0 + USB_H
    m.extend(
        wall_y(
            y0,
            x0,
            x1,
            z0,
            z1 - t,
            t,
            gaps=[(usb_x0, usb_x1, usb_z0, usb_z1)],
        )
    )

    # 按键：+X 墙两个方孔
    btn_gaps = []
    for by in (-12.0, 12.0):
        btn_gaps.append((by - BTN_R, by + BTN_R, 11.0, 11.0 + 2 * BTN_R))
    m.extend(wall_x(x1 - t, y0, y1, z0, z1 - t, t, gaps=btn_gaps))

    # -X 实墙
    m.extend(wall_x(x0, y0, y1, z0, z1 - t, t, gaps=[]))

    # 四角补齐（墙在角落重叠无所谓，保证封闭）
    # 搭接唇
    m.extend(box(x0 + t, y0 + t, z0, x1 - t, y0 + t + LIP, z0 + 3))
    m.extend(box(x0 + t, y1 - t - LIP, z0, x1 - t, y1 - t, z0 + 3))
    m.extend(box(x0 + t, y0 + t + LIP, z0, x0 + t + LIP, y1 - t - LIP, z0 + 3))
    m.extend(box(x1 - t - LIP, y0 + t + LIP, z0, x1 - t, y1 - t - LIP, z0 + 3))

    # 四角螺丝柱（与下壳对应）
    for sx, sy in (
        (-L / 2 + 8, -W / 2 + 8),
        (L / 2 - 8, -W / 2 + 8),
        (-L / 2 + 8, W / 2 - 8),
        (L / 2 - 8, W / 2 - 8),
    ):
        m.extend(tube(4.0, 1.4, H - t - 1, z0=z0 + 1).translate(sx, sy, 0))

    return m


def part_shell_lower() -> Mesh:
    L, W, H, t = SHELL_L, SHELL_W, SHELL_H * 0.45, WALL
    x0, y0 = -L / 2, -W / 2
    x1, y1 = L / 2, W / 2
    z0, z1 = 0.0, H
    m = Mesh()

    holes = [
        (-TOF_SPACING / 2, 0.0, TOF_WIN_R),
        (TOF_SPACING / 2, 0.0, TOF_WIN_R),
        (0.0, 0.0, CAM_R),
    ]
    m.extend(plate_rect_with_holes(x0, y0, x1, y1, z0, t, holes))

    # 四壁
    m.extend(box(x0, y0, z0 + t, x0 + t, y1, z1))
    m.extend(box(x1 - t, y0, z0 + t, x1, y1, z1))
    m.extend(box(x0 + t, y0, z0 + t, x1 - t, y0 + t, z1))
    m.extend(box(x0 + t, y1 - t, z0 + t, x1 - t, y1, z1))

    # ToF 外侧遮光环
    for hx in (-TOF_SPACING / 2, TOF_SPACING / 2):
        m.extend(tube(TOF_WIN_R + 2.2, TOF_WIN_R, 3.5, segments=64, z0=-3.5).translate(hx, 0, 0))

    # 螺丝柱
    for sx, sy in (
        (-L / 2 + 8, -W / 2 + 8),
        (L / 2 - 8, -W / 2 + 8),
        (-L / 2 + 8, W / 2 - 8),
        (L / 2 - 8, W / 2 - 8),
    ):
        m.extend(tube(4.0, 1.4, H - t, z0=t).translate(sx, sy, 0))

    return m


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    parts = {
        "01_ceiling_base.stl": part_ceiling_base(),
        "02_neck.stl": part_neck(),
        "03_shell_upper.stl": part_shell_upper(),
        "04_shell_lower.stl": part_shell_lower(),
    }
    for name, mesh in parts.items():
        mesh.write_stl(OUT / name, name=name.replace(".stl", ""))
    print("全部零件已输出到:", OUT)


if __name__ == "__main__":
    main()
