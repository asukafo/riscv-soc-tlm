#!/usr/bin/env python3
"""Generate SoC architecture diagram as clean SVG."""

PURPLE = "#e1d5e7"
PURPLE_S = "#9673a6"
RED = "#f8cecc"
RED_S = "#b85450"
BLUE = "#dae8fc"
BLUE_S = "#6c8ebf"
GREEN = "#d5e8d4"
GREEN_S = "#82b366"
YELLOW = "#fff2cc"
YELLOW_S = "#d6b656"
GRAY = "#f5f5f5"
GRAY_S = "#666666"
ORANGE = "#ffe6cc"
ORANGE_S = "#d79b00"
WHITE = "#ffffff"
DARK = "#333333"

W, H = 1200, 700

svg = []
def add(s):
    svg.append(s)

def rect(x, y, w, h, fill, stroke, rx=0):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>')

def txt(x, y, s, sz=11, bold=False, color=DARK, anchor="start"):
    b = "font-weight:bold;" if bold else ""
    add(f'<text x="{x}" y="{y}" font-family="Helvetica,Arial,sans-serif" font-size="{sz}" fill="{color}" text-anchor="{anchor}"><tspan style="{b}">{s}</tspan></text>')

def multiline(x, y, lines, sz=10, color=DARK, anchor="middle"):
    for i, line in enumerate(lines):
        txt(x, y + i * 14, line, sz, color=color, anchor=anchor)

def swimlane(x, y, w, h, title, bg, border):
    rect(x, y, w, h, WHITE, border, rx=6)
    rect(x+1, y+1, w-2, 24, bg, border, rx=5)
    txt(x + w/2, y + 17, title, sz=12, bold=True, anchor="middle")

def sock(x, y, w, h, lines, fill, stroke):
    rect(x, y, w, h, fill, stroke, rx=4)
    multiline(x + w/2, y + (h - len(lines)*14)/2 + 9, lines, sz=10, anchor="middle")

def arrow(x1, y1, x2, y2, color="#555555", label=""):
    mid = f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="1.5" marker-end="url(#a_{color.replace("#","")})"/>'
    add(mid)
    if label:
        txt((x1+x2)/2 + 5, (y1+y2)/2 - 4, label, sz=9, color=color)

# --- Header ---
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">')
add('<rect width="100%" height="100%" fill="white"/>')
add('<defs>')
for c in ["555555", "2255cc", "cc5522", "00aa00"]:
    add(f'<marker id="a_{c}" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="#{c}"/></marker>')
add('</defs>')

# --- Layout coordinates ---
# CPU:         x=30,  w=170
# Cache:       x=240, w=170
# Interconnect:x=450, w=220
# Memory/DMA/Disp: x=710, w=180
# y positions within each module adjusted for sockets

# === CPU ===
cx, cw = 30, 170
cpu_y = 100
cy = cpu_y
swimlane(cx, cy, cw, 180, "CPU (core.h/cpp)", BLUE, BLUE_S)
sock(cx+5, cy+35, 160, 35, ["instr_socket", "(simple_initiator)"], PURPLE, PURPLE_S)
sock(cx+5, cy+85, 160, 35, ["data_socket", "(simple_initiator)"], PURPLE, PURPLE_S)

# === Cache ===
cache_x = 240
swimlane(cache_x, cy, cw, 180, "Cache (unified I/D)", GREEN, GREEN_S)
sock(cache_x+5, cy+35, 160, 35, ["target_socket", "(multi_passthrough_target)"], RED, RED_S)
sock(cache_x+5, cy+85, 160, 35, ["initiator_socket", "(simple_initiator)"], PURPLE, PURPLE_S)

# === Interconnect ===
ic_x, ic_w = 450, 220
ic_y = 60
swimlane(ic_x, ic_y, ic_w, 430, "Interconnect", YELLOW, YELLOW_S)
sock(ic_x+5, ic_y+35, 210, 32, ["target_socket", "(multi_passthrough_target)"], RED, RED_S)
sock(ic_x+5, ic_y+90, 210, 30, ["mem_socket (simple_initiator)"], PURPLE, PURPLE_S)
sock(ic_x+5, ic_y+145, 210, 30, ["dma_mmio_socket (simple_initiator)"], PURPLE, PURPLE_S)
sock(ic_x+5, ic_y+200, 210, 30, ["display_mmio_socket (simple_initiator)"], PURPLE, PURPLE_S)
rect(ic_x+5, ic_y+255, 210, 110, WHITE, "#999999")
txt(ic_x+15, ic_y+275, "b_transport routing:", sz=10, bold=True)
txt(ic_x+20, ic_y+293, "for each region in regions[]:", sz=9, color="#666")
txt(ic_x+25, ic_y+308, "if addr in [base, base+size)", sz=9, color="#666")
txt(ic_x+30, ic_y+323, "-> forward to that socket", sz=9, color="#666")
txt(ic_x+20, ic_y+343, "else -> TLM_ADDRESS_ERROR", sz=9, color="#666")

# === Memory ===
rx, rw = 710, 180
mem_y = 80
swimlane(rx, mem_y, rw, 85, "Memory (8MB)", GRAY, GRAY_S)
sock(rx+5, mem_y+35, 170, 30, ["socket (simple_target)"], RED, RED_S)

# === DMA ===
dma_y = 200
swimlane(rx, dma_y, rw, 210, "DMA", ORANGE, ORANGE_S)
sock(rx+5, dma_y+35, 170, 32, ["target_socket MMIO", "(simple_target)"], RED, RED_S)
sock(rx+5, dma_y+85, 170, 32, ["SRC_ADDR, DST_ADDR", "SIZE, CTRL (bit0=start)"], WHITE, "#999999")
sock(rx+5, dma_y+135, 170, 32, ["initiator_socket", "(simple_initiator)"], PURPLE, PURPLE_S)

# === Display ===
disp_y = 450
swimlane(rx, disp_y, rw, 210, "Display", GREEN, PURPLE_S)
sock(rx+5, disp_y+35, 170, 32, ["target_socket MMIO", "(simple_target)"], RED, RED_S)
sock(rx+5, disp_y+85, 170, 32, ["FB_ADDR, WIDTH", "HEIGHT, CTRL (bit0=en)"], WHITE, "#999999")
sock(rx+5, disp_y+135, 170, 32, ["initiator_socket", "(simple_initiator)"], PURPLE, PURPLE_S)

# === DATA PATH ARROWS ===
# CPU -> Cache
arrow(cx+cw, cpu_y+52, cache_x, cpu_y+52, "#2255cc", "fetch")
arrow(cx+cw, cpu_y+102, cache_x, cpu_y+102, "#2255cc", "load/store")

# Cache -> Interconnect
arrow(cache_x+cw, cpu_y+102, ic_x, ic_y+51, "#2255cc")

# DMA initiator -> Interconnect (green, goes left)
arrow(rx, dma_y+151, ic_x+ic_w, ic_y+105, "#00aa00", "data transfer")

# Display initiator -> Interconnect (green, goes left)
arrow(rx, disp_y+151, ic_x+ic_w, ic_y+115, "#00aa00", "framebuffer read")

# === CONTROL PATH ARROWS (Interconnect -> targets) ===
# Interconnect -> Memory
arrow(ic_x+ic_w, ic_y+105, rx, mem_y+50, "#2255cc", "0x80000000")

# Interconnect -> DMA MMIO
arrow(ic_x+ic_w, ic_y+160, rx, dma_y+51, "#cc5522", "0x10000000")

# Interconnect -> Display MMIO
arrow(ic_x+ic_w, ic_y+215, rx, disp_y+51, "#cc5522", "0x10001000")

# === ADDRESS MAP ===
am_x, am_y = 30, 510
rect(am_x, am_y, 340, 130, WHITE, DARK, rx=6)
txt(am_x+10, am_y+22, "Address Map", sz=12, bold=True)
for i, line in enumerate([
    "0x00000000 - 0x007FFFFF  ->  Memory (low alias)",
    "0x80000000 - 0x807FFFFF  ->  Memory",
    "0x10000000 - 0x10000FFF  ->  DMA MMIO",
    "0x10001000 - 0x10001FFF  ->  Display MMIO",
]):
    txt(am_x+10, am_y+48 + i*18, line, sz=10)

# === LEGEND ===
lg_x = 390
rect(lg_x, am_y, 320, 130, WHITE, DARK, rx=6)
txt(lg_x+10, am_y+22, "Socket Types / Legend", sz=12, bold=True)
rect(lg_x+10, am_y+38, 22, 14, PURPLE, PURPLE_S, rx=3)
txt(lg_x+38, am_y+50, "simple_initiator_socket", sz=10)
txt(lg_x+38, am_y+65, "  (master, issues b_transport)", sz=9, color="#666")
rect(lg_x+10, am_y+78, 22, 14, RED, RED_S, rx=3)
txt(lg_x+38, am_y+90, "simple_target_socket / multi_passthrough_target", sz=10)
txt(lg_x+38, am_y+105, "  (slave, receives b_transport)", sz=9, color="#666")

# Arrow legend
yl = am_y + 160
add(f'<line x1="40" y1="{yl}" x2="60" y2="{yl}" stroke="#2255cc" stroke-width="1.5" marker-end="url(#a_2255cc)"/>')
txt(62, yl+4, "DATA path", sz=10, color="#2255cc")
add(f'<line x1="140" y1="{yl}" x2="160" y2="{yl}" stroke="#cc5522" stroke-width="1.5" marker-end="url(#a_cc5522)"/>')
txt(162, yl+4, "CONTROL path (MMIO)", sz=10, color="#cc5522")
add(f'<line x1="300" y1="{yl}" x2="320" y2="{yl}" stroke="#00aa00" stroke-width="1.5" marker-end="url(#a_00aa00)"/>')
txt(322, yl+4, "DMA/Display initiator path", sz=10, color="#00aa00")

add('</svg>')

with open("docs/architecture.svg", "w") as f:
    f.write("\n".join(svg))
print("Generated docs/architecture.svg")
