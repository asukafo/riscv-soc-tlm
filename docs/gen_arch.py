#!/usr/bin/env python3
"""Draw SoC architecture diagram directly to PNG using Pillow."""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 1400, 750
img = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(img)

# Colors
PURPLE, PURPLE_B = "#ede0f2", "#9673a6"
RED, RED_B = "#fde0dc", "#b85450"
BLUE, BLUE_B = "#dae8fc", "#6c8ebf"
GREEN, GREEN_B = "#d5e8d4", "#82b366"
YELLOW, YELLOW_B = "#fff2cc", "#d6b656"
GRAY, GRAY_B = "#f0f0f0", "#888888"
ORANGE, ORANGE_B = "#ffe6cc", "#d79b00"
DGREEN, DGREEN_B = "#d5e8d4", "#8a6a9e"
WHITE, DARK, LIGHT = "#ffffff", "#222222", "#999999"

def box(x, y, w, h, fill, outline, radius=6):
    d.rounded_rectangle([x, y, x+w, y+h], radius, fill=fill, outline=outline, width=2)

def txt(x, y, text, size=12, fill=DARK, bold=False, anchor="lt"):
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", size)
    except:
        font = ImageFont.load_default()
    # Simple bold: draw twice with offset (hack for basic font)
    if bold:
        d.text((x+1, y), text, fill=fill, font=font, anchor=anchor)
    d.text((x, y), text, fill=fill, font=font, anchor=anchor)

def cell(x, y, w, h, text, fill, outline, size=11, bold=False, color=DARK):
    box(x, y, w, h, fill, outline, radius=4)
    lines = text.split("\n")
    lh = size + 4
    ty = y + (h - len(lines)*lh)//2 + 2
    for i, line in enumerate(lines):
        txt(x + w//2, ty + i*lh, line, size=size, fill=color, bold=bold, anchor="mm")

def arrow(x1, y1, x2, y2, color, width=3):
    d.line([x1, y1, x2, y2], fill=color, width=width)
    # Arrowhead
    import math
    angle = math.atan2(y2-y1, x2-x1)
    L = 10
    ax = x2 - L*math.cos(angle - 0.4)
    ay = y2 - L*math.sin(angle - 0.4)
    bx = x2 - L*math.cos(angle + 0.4)
    by = y2 - L*math.sin(angle + 0.4)
    d.polygon([x2, y2, ax, ay, bx, by], fill=color)

def label(x, y, text, color, size=10):
    txt(x, y-8, text, size=size, fill=color, anchor="mm")

# ─── Layout ───
# Row 1: DATA PATH  (y=40)
# Row 2: CONTROL PATH (y=280)
# Row 3: Address Map + Legend (y=500)

# === ROW 1: DATA PATH ===
y1 = 40
# CPU
cpu_x, cpu_w = 30, 160
box(cpu_x, y1, cpu_w, 170, WHITE, BLUE_B, radius=8)
box(cpu_x, y1, cpu_w, 26, BLUE, BLUE_B, radius=8)
txt(cpu_x+cpu_w//2, y1+14, "CPU (core.h/cpp)", size=12, bold=True, anchor="mm")

cell(cpu_x+5, y1+35, 150, 42, "instr_socket\nsimple_initiator", PURPLE, PURPLE_B, size=10)
cell(cpu_x+5, y1+90, 150, 42, "data_socket\nsimple_initiator", PURPLE, PURPLE_B, size=10)

# Cache
cache_x = cpu_x + cpu_w + 70
cache_w = 170
box(cache_x, y1, cache_w, 170, WHITE, GREEN_B, radius=8)
box(cache_x, y1, cache_w, 26, GREEN, GREEN_B, radius=8)
txt(cache_x+cache_w//2, y1+14, "Cache (unified I/D)", size=12, bold=True, anchor="mm")

cell(cache_x+5, y1+35, 160, 42, "target_socket\nmulti_passthrough_target", RED, RED_B, size=10)
cell(cache_x+5, y1+90, 160, 42, "initiator_socket\nsimple_initiator", PURPLE, PURPLE_B, size=10)

# Arrows CPU -> Cache
arr1_y = y1 + 56
arrow(cpu_x+cpu_w, arr1_y, cache_x, arr1_y, "#2255cc")
label((cpu_x+cpu_w+cache_x)//2, arr1_y, "fetch", "#2255cc", size=9)
arr2_y = y1 + 111
arrow(cpu_x+cpu_w, arr2_y, cache_x, arr2_y, "#2255cc")
label((cpu_x+cpu_w+cache_x)//2, arr2_y, "load/store", "#2255cc", size=9)

# Interconnect (right side of cache)
ic_x = cache_x + cache_w + 70
ic_w = 240
box(ic_x, y1-20, ic_w, 380, WHITE, YELLOW_B, radius=8)
box(ic_x, y1-20, ic_w, 26, YELLOW, YELLOW_B, radius=8)
txt(ic_x+ic_w//2, y1-6, "Interconnect", size=13, bold=True, anchor="mm")

cell(ic_x+5, y1+20, 230, 38, "target_socket\nmulti_passthrough_target", RED, RED_B, size=10)
cell(ic_x+5, y1+75, 230, 32, "mem_socket  (simple_initiator)", PURPLE, PURPLE_B, size=10)
cell(ic_x+5, y1+125, 230, 32, "dma_mmio_socket  (simple_initiator)", PURPLE, PURPLE_B, size=10)
cell(ic_x+5, y1+175, 230, 32, "display_mmio_socket  (simple_initiator)", PURPLE, PURPLE_B, size=10)

# Routing logic inside Interconnect
box(ic_x+5, y1+225, 230, 60, WHITE, LIGHT, radius=0)
txt(ic_x+12, y1+232, "b_transport routing:", size=11, fill=DARK, bold=True)
txt(ic_x+15, y1+250, "for each AddressRegion:", size=10, fill=LIGHT)
txt(ic_x+20, y1+268, "addr in [base,base+size) -> forward", size=10, fill=LIGHT)

# Row for interconnect output labels (between IC and targets)
# Arrow: cache initiator -> IC target
arr3_y = y1 + 111
arrow(cache_x+cache_w, arr3_y+4, ic_x, y1+39, "#2255cc")

# Memory
mem_x = ic_x + ic_w + 60
mem_w = 170
mem_y = y1 - 20
box(mem_x, mem_y, mem_w, 100, WHITE, GRAY_B, radius=8)
box(mem_x, mem_y, mem_w, 26, GRAY, GRAY_B, radius=8)
txt(mem_x+mem_w//2, mem_y+14, "Memory (8MB)", size=12, bold=True, anchor="mm")
cell(mem_x+5, mem_y+35, 160, 34, "socket  (simple_target)", RED, RED_B, size=10)

# DMA
dma_x = mem_x
dma_w = mem_w
dma_y = mem_y + 125
box(dma_x, dma_y, dma_w, 200, WHITE, ORANGE_B, radius=8)
box(dma_x, dma_y, dma_w, 26, ORANGE, ORANGE_B, radius=8)
txt(dma_x+dma_w//2, dma_y+14, "DMA", size=12, bold=True, anchor="mm")
cell(dma_x+5, dma_y+35, 160, 38, "target_socket MMIO\nsimple_target", RED, RED_B, size=10)
cell(dma_x+5, dma_y+85, 160, 28, "SRC,DST,SIZE,CTRL[0]=start", WHITE, LIGHT, size=9)
cell(dma_x+5, dma_y+128, 160, 38, "initiator_socket\nsimple_initiator", PURPLE, PURPLE_B, size=10)

# Display
disp_x = dma_x + dma_w + 20
disp_w = dma_w
disp_y = mem_y
box(disp_x, disp_y, disp_w, 200, WHITE, DGREEN_B, radius=8)
box(disp_x, disp_y, disp_w, 26, DGREEN, DGREEN_B, radius=8)
txt(disp_x+disp_w//2, disp_y+14, "Display (LT)", size=12, bold=True, anchor="mm")
cell(disp_x+5, disp_y+35, 160, 30, "target_socket MMIO\nsimple_target", RED, RED_B, size=10)
cell(disp_x+5, disp_y+78, 160, 28, "FB,W,H,CTRL[0]=enable", WHITE, LIGHT, size=9)
cell(disp_x+5, disp_y+120, 160, 38, "initiator_socket\nsimple_initiator", PURPLE, PURPLE_B, size=10)

# Control arrows: IC -> targets
arrow(ic_x+ic_w, y1+91, mem_x, mem_y+52, "#cc5522")
label(ic_x+ic_w+25, y1+95, "0x80000000", "#cc5522", size=9)

arrow(ic_x+ic_w, y1+141, dma_x, dma_y+54, "#cc5522")
label(ic_x+ic_w+25, y1+145, "0x10000000", "#cc5522", size=9)

arrow(ic_x+ic_w, y1+191, disp_x, disp_y+50, "#cc5522")
label(ic_x+ic_w+25, y1+195, "0x10001000", "#cc5522", size=9)

# Green arrows: DMA/Display initiator -> IC
g_arr_y = y1 + 39 + 15
arrow(dma_x, dma_y+147, ic_x+ic_w, g_arr_y+35, "#008800")
label(dma_x-60, dma_y+120, "data transfer", "#008800", size=9)
arrow(disp_x, disp_y+139, ic_x+ic_w, g_arr_y+55, "#008800")
label(disp_x-70, disp_y+110, "fb read", "#008800", size=9)

# === ADDRESS MAP + LEGEND (bottom) ===
bot_y = 480
# Address map
box(30, bot_y, 440, 150, WHITE, DARK, radius=6)
txt(45, bot_y+14, "Address Map", size=13, bold=True, anchor="lt")
addr_lines = [
    "0x00000000 - 0x007FFFFF   ->   Memory (low alias)",
    "0x80000000 - 0x807FFFFF   ->   Memory",
    "0x10000000 - 0x10000FFF   ->   DMA MMIO",
    "0x10001000 - 0x10001FFF   ->   Display MMIO",
]
for i, line in enumerate(addr_lines):
    txt(45, bot_y+42+i*20, line, size=11, fill=DARK)

# Legend
leg_x = 500
box(leg_x, bot_y, 420, 150, WHITE, DARK, radius=6)
txt(leg_x+15, bot_y+14, "Legend", size=13, bold=True, anchor="lt")
box(leg_x+15, bot_y+38, 20, 14, PURPLE, PURPLE_B, radius=3)
txt(leg_x+42, bot_y+42, "simple_initiator_socket = master, issues b_transport", size=11)
box(leg_x+15, bot_y+62, 20, 14, RED, RED_B, radius=3)
txt(leg_x+42, bot_y+66, "simple_target_socket / multi_passthrough = slave", size=11)

# Arrow legend
d.line([leg_x+15, bot_y+95, leg_x+55, bot_y+95], fill="#2255cc", width=3)
txt(leg_x+62, bot_y+98, "DATA path", size=11, fill="#2255cc")
d.line([leg_x+140, bot_y+95, leg_x+180, bot_y+95], fill="#cc5522", width=3)
txt(leg_x+187, bot_y+98, "CONTROL path (MMIO)", size=11, fill="#cc5522")
d.line([leg_x+310, bot_y+95, leg_x+350, bot_y+95], fill="#008800", width=3)
txt(leg_x+357, bot_y+98, "Device initiator", size=11, fill="#008800")

# Save
img.save("docs/architecture.png")
print(f"Generated docs/architecture.png ({W}x{H})")
