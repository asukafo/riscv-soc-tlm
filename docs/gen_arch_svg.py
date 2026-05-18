#!/usr/bin/env python3
"""Generate architecture diagram as SVG."""

import textwrap

W, H = 1200, 700

COLORS = {
    "cpu_bg": "#dae8fc", "cpu_border": "#6c8ebf",
    "cache_bg": "#d5e8d4", "cache_border": "#82b366",
    "ic_bg": "#fff2cc", "ic_border": "#d6b656",
    "mem_bg": "#f5f5f5", "mem_border": "#666666",
    "dma_bg": "#ffe6cc", "dma_border": "#d79b00",
    "disp_bg": "#d5e8d4", "disp_border": "#9673a6",
    "initiator": "#e1d5e7", "initiator_border": "#9673a6",
    "target": "#f8cecc", "target_border": "#b85450",
    "legend_bg": "#ffffff", "legend_border": "#333333",
    "arrow": "#555555", "arrow_data": "#2255cc", "arrow_ctrl": "#cc5522",
}


def sw(x): return x  # no scaling

def box(x, y, w, h, fill, stroke, rx=4):
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
            f'rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>')

def text(x, y, s, size=11, bold=False, color="#333333", anchor="start"):
    b = "font-weight:bold;" if bold else ""
    return (f'<text x="{x}" y="{y}" font-family="Helvetica,Arial,sans-serif" '
            f'font-size="{size}" fill="{color}" text-anchor="{anchor}" style="{b}">{s}</text>')

def socket_box(x, y, w, h, label, fill, border, font_size=10):
    lines = label.split("\n")
    out = [box(x, y, w, h, fill, border, rx=3)]
    lh = 14
    sy = y + (h - len(lines) * lh) / 2 + lh - 2
    for i, line in enumerate(lines):
        out.append(text(x + w/2, sy + i * lh, line, size=font_size, anchor="middle"))
    return out

def arrow(x1, y1, x2, y2, label="", color=COLORS["arrow"], lw=1.5):
    out = []
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    out.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
               f'stroke="{color}" stroke-width="{lw}" marker-end="url(#arrowhead_{color.replace("#","")})"/>')
    if label:
        out.append(text(mx + 5, my - 5, label, size=9, color=color))
    return out

def swimlane(x, y, w, h, title, bg, border):
    out = []
    out.append(box(x, y, w, h, "#ffffff", border, rx=6))
    out.append(f'<rect x="{x}" y="{y}" width="{w}" height="26" rx="6" fill="{bg}" stroke="{border}" stroke-width="1.2"/>')
    out.append(f'<path d="M{x} {y+20} L{x+w} {y+20}" stroke="{border}" stroke-width="1"/>')
    out.append(box(x, y+26, w, h-26, bg, border, rx=0))
    out.append(box(x, y+h-2, w, 2, bg, border, rx=6))
    out.append(text(x + w/2, y + 18, title, size=12, bold=True, anchor="middle"))
    return out

parts = []
def add(*s):
    parts.extend(s)

# Arrow marker defs
add(
    '<defs>',
    '<marker id="arrowhead_555555" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">',
    '<polygon points="0 0, 8 3, 0 6" fill="#555555"/>', '</marker>',
    '<marker id="arrowhead_2255cc" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">',
    '<polygon points="0 0, 8 3, 0 6" fill="#2255cc"/>', '</marker>',
    '<marker id="arrowhead_cc5522" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">',
    '<polygon points="0 0, 8 3, 0 6" fill="#cc5522"/>', '</marker>',
    '<marker id="arrowhead_00aa00" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">',
    '<polygon points="0 0, 8 3, 0 6" fill="#00aa00"/>', '</marker>',
    '</defs>'
)

# ─── Layout ───
lx = 30   # left margin
cl, cache_l, ic_l, right_l = lx, lx+190, lx+390, lx+630

# ─── CPU ───
cpu_y, cpu_w, cpu_h = 120, 160, 160
add(*swimlane(cl, cpu_y, cpu_w, cpu_h, "CPU (core.h/cpp)", COLORS["cpu_bg"], COLORS["cpu_border"]))
add(*socket_box(cl+5, cpu_y+40, cpu_w-10, 35, "instr_socket\n(simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))
add(*socket_box(cl+5, cpu_y+95, cpu_w-10, 35, "data_socket\n(simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))

# ─── Cache ───
cache_y = 120
add(*swimlane(cache_l, cache_y, cpu_w, cpu_h, "Cache (unified I/D)", COLORS["cache_bg"], COLORS["cache_border"]))
add(*socket_box(cache_l+5, cache_y+40, cpu_w-10, 35, "target_socket\n(multi_passthrough_target)", COLORS["target"], COLORS["target_border"]))
add(*socket_box(cache_l+5, cache_y+95, cpu_w-10, 35, "initiator_socket\n(simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))

# ─── Interconnect ───
ic_y, ic_w, ic_h = 80, 210, 400
add(*swimlane(ic_l, ic_y, ic_w, ic_h, "Interconnect", COLORS["ic_bg"], COLORS["ic_border"]))
add(*socket_box(ic_l+5, ic_y+40, ic_w-10, 30, "target_socket\n(multi_passthrough_target)", COLORS["target"], COLORS["target_border"]))
add(*socket_box(ic_l+5, ic_y+95, ic_w-10, 30, "mem_socket (simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))
add(*socket_box(ic_l+5, ic_y+150, ic_w-10, 30, "dma_mmio_socket (simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))
add(*socket_box(ic_l+5, ic_y+205, ic_w-10, 30, "display_mmio_socket (simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))
# Routing logic
add(box(ic_l+5, ic_y+260, ic_w-10, 80, "#ffffff", "#999999", rx=0))
add(text(ic_l+15, ic_y+280, "b_transport routing:", size=10, bold=True))
add(text(ic_l+15, ic_y+297, "for each AddressRegion:", size=9, color="#666666"))
add(text(ic_l+20, ic_y+312, "if addr in [base, base+size) -> forward to socket", size=9, color="#666666"))
add(text(ic_l+20, ic_y+327, "else -> TLM_ADDRESS_ERROR_RESPONSE", size=9, color="#666666"))

# ─── Memory ───
mem_y = 100
add(*swimlane(right_l, mem_y, 170, 85, "Memory (8MB)", COLORS["mem_bg"], COLORS["mem_border"]))
add(*socket_box(right_l+5, mem_y+35, 160, 30, "socket (simple_target)", COLORS["target"], COLORS["target_border"]))

# ─── DMA ───
dma_y = 230
add(*swimlane(right_l, dma_y, 170, 180, "DMA", COLORS["dma_bg"], COLORS["dma_border"]))
add(*socket_box(right_l+5, dma_y+35, 160, 30, "target_socket MMIO\n(simple_target)", COLORS["target"], COLORS["target_border"]))
add(*socket_box(right_l+5, dma_y+110, 160, 30, "initiator_socket\n(simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))

# ─── Display ───
disp_y = 455
add(*swimlane(right_l, disp_y, 170, 180, "Display", COLORS["disp_bg"], COLORS["disp_border"]))
add(*socket_box(right_l+5, disp_y+35, 160, 30, "target_socket MMIO\n(simple_target)", COLORS["target"], COLORS["target_border"]))
add(*socket_box(right_l+5, disp_y+110, 160, 30, "initiator_socket\n(simple_initiator)", COLORS["initiator"], COLORS["initiator_border"]))

# ─── Arrows: CPU -> Cache ───
add(*arrow(cl+cpu_w, cache_y+57, cache_l, cache_y+57, "", COLORS["arrow_data"]))
add(*arrow(cl+cpu_w, cache_y+112, cache_l, cache_y+112, "", COLORS["arrow_data"]))

# ─── Arrow: Cache -> Interconnect ───
add(*arrow(cache_l+cpu_w, cache_y+112, ic_l, ic_y+55, "", COLORS["arrow_data"]))

# ─── Arrows: DMA/Display -> Interconnect (data initiator path) ───
dma_green = "#00aa00"
ic_mid_x = ic_l - 10
dma_init_y = dma_y + 125
disp_init_y = disp_y + 125
add(*arrow(right_l, dma_init_y, ic_mid_x, ic_y+55, "", dma_green))
add(*arrow(right_l, disp_init_y, ic_mid_x, ic_y+65, "", dma_green))

# ─── Arrows: Interconnect -> Memory ───
add(*arrow(ic_l+ic_w, ic_y+110, right_l, mem_y+50, "0x80000000", COLORS["arrow_data"]))

# ─── Arrows: Interconnect -> DMA MMIO ───
add(*arrow(ic_l+ic_w, ic_y+165, right_l, dma_y+50, "0x10000000", COLORS["arrow_ctrl"]))

# ─── Arrows: Interconnect -> Display MMIO ───
add(*arrow(ic_l+ic_w, ic_y+220, right_l, disp_y+50, "0x10001000", COLORS["arrow_ctrl"]))

# ─── Address Map ───
am_x, am_y, am_w = lx, 510, 320
add(box(am_x, am_y, am_w, 130, "#ffffff", COLORS["legend_border"], rx=6))
add(text(am_x+10, am_y+22, "Address Map", size=11, bold=True))
add(text(am_x+10, am_y+45, "0x00000000 - 0x007FFFFF  ->  Memory (low alias)", size=10))
add(text(am_x+10, am_y+65, "0x80000000 - 0x807FFFFF  ->  Memory", size=10))
add(text(am_x+10, am_y+85, "0x10000000 - 0x10000FFF  ->  DMA MMIO", size=10))
add(text(am_x+10, am_y+105, "0x10001000 - 0x10001FFF  ->  Display MMIO", size=10))

# ─── Legend ───
lg_x, lg_y = lx+340, 510
add(box(lg_x, lg_y, 280, 130, "#ffffff", COLORS["legend_border"], rx=6))
add(text(lg_x+10, lg_y+22, "Socket Types / Legend", size=11, bold=True))
add(box(lg_x+10, lg_y+35, 24, 16, COLORS["initiator"], COLORS["initiator_border"], rx=3))
add(text(lg_x+40, lg_y+48, "simple_initiator_socket  (master, issues b_transport)", size=10))
add(box(lg_x+10, lg_y+60, 24, 16, COLORS["target"], COLORS["target_border"], rx=3))
add(text(lg_x+40, lg_y+73, "simple_target_socket / multi_passthrough_target  (slave)", size=10))
# Arrow legends
add(f'<line x1="{lg_x+10}" y1="{lg_y+92}" x2="{lg_x+30}" y2="{lg_y+92}" '
    f'stroke="{COLORS["arrow_data"]}" stroke-width="1.5" marker-end="url(#arrowhead_2255cc)"/>')
add(text(lg_x+30, lg_y+97, "DATA path", size=9, color=COLORS["arrow_data"]))
add(f'<line x1="{lg_x+70}" y1="{lg_y+92}" x2="{lg_x+90}" y2="{lg_y+92}" '
    f'stroke="{COLORS["arrow_ctrl"]}" stroke-width="1.5" marker-end="url(#arrowhead_cc5522)"/>')
add(text(lg_x+90, lg_y+97, "CONTROL path", size=9, color=COLORS["arrow_ctrl"]))
add(f'<line x1="{lg_x+170}" y1="{lg_y+92}" x2="{lg_x+190}" y2="{lg_y+92}" '
    f'stroke="#00aa00" stroke-width="1.5" marker-end="url(#arrowhead_00aa00)"/>')
add(text(lg_x+190, lg_y+97, "DMA/Display initiator", size=9, color="#00aa00"))

# Data/Control path labels
cx = lx+30
cy = am_y+145
add(text(cx, cy, "Data path:   CPU -> Cache -> Interconnect -> Memory  |  DMA/Display -> Interconnect -> Memory", size=10, color=COLORS["arrow_data"]))
add(text(cx, cy+18, "Control path:  CPU -> Cache -> Interconnect -> DMA MMIO / Display MMIO", size=10, color=COLORS["arrow_ctrl"]))

# Build SVG
svg_parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
             f'width="{W}" height="{H}">']
svg_parts.append('<rect width="100%" height="100%" fill="white"/>')
svg_parts.extend(parts)
svg_parts.append('</svg>')

with open("docs/architecture.svg", "w") as f:
    f.write("\n".join(svg_parts))

print("Generated docs/architecture.svg")
