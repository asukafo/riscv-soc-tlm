#!/usr/bin/env python3
"""Generate architecture diagram as standalone HTML page."""

html = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>RISC-V SoC Architecture — Socket Topology</title>
<style>
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family: Helvetica,Arial,sans-serif; background:#fff; padding:30px; color:#333; }
h1 { font-size:18px; margin-bottom:24px; }

.diagram { position:relative; width:1200px; }
.swimlane { position:absolute; border:1.5px solid; border-radius:6px; background:#fff; }
.swimlane .title { position:absolute; left:0; top:0; right:0; height:24px; border-radius:5px 5px 0 0;
    text-align:center; font-size:12px; font-weight:bold; line-height:24px; }
.sock { position:absolute; border:1.5px solid; border-radius:4px; text-align:center;
    font-size:10px; display:flex; flex-direction:column; justify-content:center; line-height:1.3; }

.arrow-v { position:absolute; pointer-events:none; }
.arrow-v svg { overflow:visible; }

.legend-box { display:inline-block; width:16px; height:12px; border-radius:3px; border:1.5px solid; vertical-align:middle; margin-right:4px; }
.legend-line { display:inline-block; width:18px; height:0; border-top:1.5px solid; vertical-align:middle; margin:0 4px; position:relative; }
.legend-line::after { content:''; position:absolute; right:-4px; top:-3px;
    border:3px solid transparent; border-left-width:6px; }

.map-table { border-collapse:collapse; font-size:11px; }
.map-table td { padding:2px 8px; }
.map-table tr:first-child td { font-weight:bold; padding-bottom:6px; }
</style>
</head>
<body>
<h1>RISC-V SoC Architecture — Socket Topology</h1>

<div class="diagram">
"""

def swimlane(x, y, w, h, title, bg, border):
    return (f'<div class="swimlane" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;'
            f'border-color:{border};">'
            f'<div class="title" style="background:{bg};border-bottom:1px solid {border};">{title}</div>')

def sock_div(x, y, w, h, text, bg, border, parent_w=None):
    # x is relative to parent right edge + gap, or absolute within swimlane
    return (f'<div class="sock" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;'
            f'background:{bg};border-color:{border};">{text}</div>')

# Close swimlane div
def sw_end():
    return '</div>'

def arrow_h(x1, y1, x2, y2, color, label=""):
    mid = (y1 + y2) // 2
    w = x2 - x1
    svg = (f'<svg style="position:absolute;left:{x1}px;top:{mid-7}px;width:{w}px;height:14px;overflow:visible">'
           f'<line x1="0" y1="7" x2="{w-4}" y2="7" stroke="{color}" stroke-width="1.5"/>'
           f'<polygon points="{w-10},2 {w},7 {w-10},12" fill="{color}"/>')
    if label:
        svg += f'<text x="{w//2}" y="2" font-size="9" fill="{color}" text-anchor="middle">{label}</text>'
    svg += '</svg>'
    return svg

# BODY padding = 30px. Offsets relative to diagram div
GUTTER = 0

# Build sections using absolute positioning
# Column layout: CPU(30-200) | Cache(230-400) | Interconnect(430-650) | Memory/DMA/Display(680-880)
C1, C2, C3, C4 = 30, 240, 460, 690

# === CPU ===
html += swimlane(C1, 80, 170, 170, "CPU (core.h/cpp)", "#dae8fc", "#6c8ebf")
html += sock_div(5, 35, 160, 35, "instr_socket<br>(simple_initiator)", "#e1d5e7", "#9673a6")
html += sock_div(5, 85, 160, 35, "data_socket<br>(simple_initiator)", "#e1d5e7", "#9673a6")
html += sw_end()

# === Cache ===
html += swimlane(C2, 80, 170, 170, "Cache (unified I/D)", "#d5e8d4", "#82b366")
html += sock_div(5, 35, 160, 35, "target_socket<br>(multi_passthrough_target)", "#f8cecc", "#b85450")
html += sock_div(5, 85, 160, 35, "initiator_socket<br>(simple_initiator)", "#e1d5e7", "#9673a6")
html += sw_end()

# === Interconnect ===
html += swimlane(C3, 40, 220, 470, "Interconnect", "#fff2cc", "#d6b656")
html += sock_div(5, 35, 210, 32, "target_socket<br>(multi_passthrough_target)", "#f8cecc", "#b85450")
html += sock_div(5, 85, 210, 28, "mem_socket (simple_initiator)", "#e1d5e7", "#9673a6")
html += sock_div(5, 130, 210, 28, "dma_mmio_socket (simple_initiator)", "#e1d5e7", "#9673a6")
html += sock_div(5, 175, 210, 28, "display_mmio_socket (simple_initiator)", "#e1d5e7", "#9673a6")

# routing info inside interconnect
html += (f'<div style="position:absolute;left:5px;top:225px;width:210px;height:110px;'
        f'border:1px solid #999;font-size:9px;padding:6px;line-height:1.4;">'
        f'<b>b_transport routing:</b><br>'
        f'&nbsp;for each region in regions[]:<br>'
        f'&nbsp;&nbsp;if addr in [base, base+size)<br>'
        f'&nbsp;&nbsp;&nbsp;&nbsp;-> forward to that socket<br>'
        f'&nbsp;else -> TLM_ADDRESS_ERROR</div>')
html += sw_end()

# === Memory ===
html += swimlane(C4, 60, 180, 80, "Memory (8MB)", "#f5f5f5", "#666666")
html += sock_div(5, 35, 170, 26, "socket (simple_target)", "#f8cecc", "#b85450")
html += sw_end()

# === DMA ===
html += swimlane(C4, 170, 180, 200, "DMA", "#ffe6cc", "#d79b00")
html += sock_div(5, 35, 170, 30, "target_socket MMIO (simple_target)", "#f8cecc", "#b85450")
html += (f'<div class="sock" style="left:5px;top:80px;width:170px;height:30px;'
        f'background:#fff;border-color:#999;">SRC_ADDR, DST_ADDR, SIZE, CTRL[0]=start</div>')
html += sock_div(5, 125, 170, 30, "initiator_socket (simple_initiator)", "#e1d5e7", "#9673a6")
html += sw_end()

# === Display ===
html += swimlane(C4, 400, 180, 200, "Display", "#d5e8d4", "#9673a6")
html += sock_div(5, 35, 170, 30, "target_socket MMIO (simple_target)", "#f8cecc", "#b85450")
html += (f'<div class="sock" style="left:5px;top:80px;width:170px;height:30px;'
        f'background:#fff;border-color:#999;">FB_ADDR, WIDTH, HEIGHT, CTRL[0]=enable</div>')
html += sock_div(5, 125, 170, 30, "initiator_socket (simple_initiator)", "#e1d5e7", "#9673a6")
html += sw_end()

# === ARROWS (SVG overlay) ===
# CPU -> Cache (instr)
html += arrow_h(C1+170, 117, C2, 117, "#2255cc", "fetch")
# CPU -> Cache (data)
html += arrow_h(C1+170, 167, C2, 167, "#2255cc", "load/store")
# Cache -> Interconnect
html += arrow_h(C2+170, 167, C3, 96, "#2255cc", "")
# DMA initiator -> Interconnect (backwards)
html += arrow_h(C4, 336, C3+220, 113, "#00aa00", "data transfer")
# Display initiator -> Interconnect (backwards)
html += arrow_h(C4, 566, C3+220, 123, "#00aa00", "framebuffer read")
# Interconnect -> Memory
html += arrow_h(C3+220, 107, C4, 120, "#2255cc", "0x80000000")
# Interconnect -> DMA MMIO
html += arrow_h(C3+220, 148, C4, 204, "#cc5522", "0x10000000")
# Interconnect -> Display MMIO
html += arrow_h(C3+220, 193, C4, 434, "#cc5522", "0x10001000")

# === ADDRESS MAP ===
map_y = 640
html += (f'<div style="position:absolute;left:{C1}px;top:{map_y}px;width:360px;'
        f'border:1.5px solid #333;border-radius:6px;padding:12px;background:#fff;">')
html += '<b>Address Map</b><br>'
html += '<table class="map-table">'
for addr, target in [
    ("0x00000000 - 0x007FFFFF", "Memory (low alias)"),
    ("0x80000000 - 0x807FFFFF", "Memory"),
    ("0x10000000 - 0x10000FFF", "DMA MMIO"),
    ("0x10001000 - 0x10001FFF", "Display MMIO"),
]:
    html += f'<tr><td style="font-family:monospace">{addr}</td><td>-></td><td>{target}</td></tr>'
html += '</table></div>'

# === LEGEND ===
html += (f'<div style="position:absolute;left:410px;top:{map_y}px;width:350px;'
        f'border:1.5px solid #333;border-radius:6px;padding:12px;background:#fff;">')
html += '<b>Socket Types</b><br>'
html += '<div style="margin-top:6px;font-size:10px;line-height:1.6;">'
html += '<span class="legend-box" style="background:#e1d5e7;border-color:#9673a6;"></span> '
html += '<b>simple_initiator_socket</b> — master, initiates b_transport<br>'
html += '<span class="legend-box" style="background:#f8cecc;border-color:#b85450;"></span> '
html += '<b>simple_target_socket</b> / <b>multi_passthrough_target</b> — slave, receives<br>'
html += '</div>'
html += '<div style="margin-top:8px;font-size:10px;line-height:1.8;">'
html += '<b>Arrow colors:</b><br>'
html += '<span class="legend-line" style="border-color:#2255cc;"></span> <span style="color:#2255cc;">Data path</span>&nbsp;&nbsp;'
html += '<span class="legend-line" style="border-color:#cc5522;"></span> <span style="color:#cc5522;">Control path (MMIO)</span>&nbsp;&nbsp;'
html += '<span class="legend-line" style="border-color:#00aa00;"></span> <span style="color:#00aa00;">DMA/Display initiator</span>'
html += '</div></div>'

html += "\n</div>\n</body>\n</html>"

with open("docs/architecture.html", "w") as f:
    f.write(html)
print("Generated docs/architecture.html")
