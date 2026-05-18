#!/usr/bin/env python3
"""Generate architecture diagram using Graphviz (automatic layout)."""
import graphviz

dot = graphviz.Digraph("SoC", format="png", engine="dot")
dot.attr(rankdir="LR", splines="polyline", nodesep="0.6", ranksep="0.8",
         fontname="Helvetica", bgcolor="white")
dot.attr("node", fontname="Helvetica", fontsize="11", margin="0.05")
dot.attr("edge", fontname="Helvetica", fontsize="9")

# --- CPU ---
dot.node("cpu", "CPU\ncore.h/cpp", shape="box", style="filled",
         fillcolor="#dae8fc", color="#6c8ebf")
dot.node("cpu_is", "instr_socket\n(simple_initiator)", shape="box",
         style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")
dot.node("cpu_ds", "data_socket\n(simple_initiator)", shape="box",
         style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")
dot.edge("cpu", "cpu_is", style="invis")
dot.edge("cpu", "cpu_ds", style="invis")

# --- Cache ---
dot.node("cache", "Cache\n(unified I/D)", shape="box", style="filled",
         fillcolor="#d5e8d4", color="#82b366")
dot.node("cache_ts", "target_socket\n(multi_passthrough_target)", shape="box",
         style="filled,rounded", fillcolor="#f8cecc", color="#b85450", fontsize="10")
dot.node("cache_is", "initiator_socket\n(simple_initiator)", shape="box",
         style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")
dot.edge("cache", "cache_ts", style="invis")
dot.edge("cache", "cache_is", style="invis")

# --- Interconnect ---
with dot.subgraph(name="cluster_ic") as ic:
    ic.attr(label="Interconnect", style="filled", fillcolor="#fff2cc",
            color="#d6b656", fontname="Helvetica", fontsize="12")
    ic.node("ic_ts", "target_socket\n(multi_passthrough_target)", shape="box",
            style="filled,rounded", fillcolor="#f8cecc", color="#b85450", fontsize="10")
    ic.node("ic_mem", "mem_socket\n(simple_initiator)", shape="box",
            style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")
    ic.node("ic_dma", "dma_mmio_socket\n(simple_initiator)", shape="box",
            style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")
    ic.node("ic_disp", "display_mmio_socket\n(simple_initiator)", shape="box",
            style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")

# --- Memory ---
dot.node("mem", "Memory (8MB)\nsocket (simple_target)", shape="box",
         style="filled", fillcolor="#f5f5f5", color="#666666")

# --- DMA ---
with dot.subgraph(name="cluster_dma") as dma:
    dma.attr(label="DMA", style="filled", fillcolor="#ffe6cc",
             color="#d79b00", fontname="Helvetica", fontsize="12")
    dma.node("dma_ts", "target_socket MMIO\n(simple_target)", shape="box",
             style="filled,rounded", fillcolor="#f8cecc", color="#b85450", fontsize="10")
    dma.node("dma_is", "initiator_socket\n(simple_initiator)", shape="box",
             style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")

# --- Display ---
with dot.subgraph(name="cluster_disp") as disp:
    disp.attr(label="Display", style="filled", fillcolor="#d5e8d4",
              color="#9673a6", fontname="Helvetica", fontsize="12")
    disp.node("disp_ts", "target_socket MMIO\n(simple_target)", shape="box",
              style="filled,rounded", fillcolor="#f8cecc", color="#b85450", fontsize="10")
    disp.node("disp_is", "initiator_socket\n(simple_initiator)", shape="box",
              style="filled,rounded", fillcolor="#e1d5e7", color="#9673a6", fontsize="10")

# === DATA PATH EDGES (blue) ===
# CPU -> Cache
dot.edge("cpu_is", "cache_ts", "fetch", color="#2255cc", fontcolor="#2255cc")
dot.edge("cpu_ds", "cache_ts", "load/store", color="#2255cc", fontcolor="#2255cc")
# Cache -> Interconnect
dot.edge("cache_is", "ic_ts", color="#2255cc")
# DMA initiator -> Interconnect
dot.edge("dma_is", "ic_ts", "data\ntransfer", color="#00aa00", fontcolor="#00aa00",
         dir="back")
# Display initiator -> Interconnect
dot.edge("disp_is", "ic_ts", "framebuffer\nread", color="#00aa00", fontcolor="#00aa00",
         dir="back")

# === CONTROL PATH EDGES (orange) ===
dot.edge("ic_mem", "mem", "0x80000000", color="#cc5522", fontcolor="#cc5522")
dot.edge("ic_dma", "dma_ts", "0x10000000", color="#cc5522", fontcolor="#cc5522")
dot.edge("ic_disp", "disp_ts", "0x10001000", color="#cc5522", fontcolor="#cc5522")

# === ADDRESS MAP (table as node) ===
addr_map = ("Address Map\n"
            "0x00000000-0x007FFFFF -> Memory (low alias)\n"
            "0x80000000-0x807FFFFF -> Memory\n"
            "0x10000000-0x10000FFF -> DMA MMIO\n"
            "0x10001000-0x10001FFF -> Display MMIO")
dot.node("addr_map", addr_map, shape="note", fontsize="9", style="filled",
         fillcolor="#ffffff", color="#333333")

# === LEGEND ===
legend = ("LEGEND\n"
          "[purple] simple_initiator = master\n"
          "[red] simple_target = slave\n"
          "[blue] arrow = DATA path\n"
          "[orange] arrow = CONTROL path")
dot.node("legend", legend, shape="note", fontsize="9", style="filled",
         fillcolor="#ffffff", color="#333333")

# Render
dot.render("docs/architecture", cleanup=True)
print("Generated docs/architecture.png")
