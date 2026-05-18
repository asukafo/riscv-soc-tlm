#!/usr/bin/env python3
"""Generate SoC architecture diagram — clean HTML table layout."""
CSS = "body{font:11px Menlo,Monaco,monospace;margin:24px;color:#222;background:#fff} h2{font-size:14px;margin:0 0 12px} table{border-collapse:collapse;margin-bottom:16px} td{border:1px solid #ddd;padding:6px 12px;vertical-align:middle;text-align:center;white-space:nowrap} .hdr{font-weight:bold;font-size:11px} .i{background:#ede0f2;border:1.5px solid #9673a6} .t{background:#fde0dc;border:1.5px solid #b85450} .m{background:#dae8fc;border:1.5px solid #6c8ebf} .c{background:#d5e8d4;border:1.5px solid #82b366} .b{background:#fff2cc;border:1.5px solid #d6b656} .mm{background:#f0f0f0;border:1.5px solid #888} .d{background:#ffe6cc;border:1.5px solid #d79b00} .p{background:#e0e8d0;border:1.5px solid #8a6a9e} .ba{font-weight:bold;color:#2255cc} .oa{font-weight:bold;color:#cc5522} .ga{font-weight:bold;color:#008800} .na{color:#999;font-size:9px}"

HTML = f'''<!DOCTYPE html><html><head><meta charset="UTF-8"><title>SoC Architecture</title><style>{CSS}</style></head><body>
<h2>RISC-V SoC TLM-2.0 Architecture</h2>

<table>
<tr>
  <td class="hdr m">CPU</td><td></td>
  <td class="hdr c">Cache</td><td></td>
  <td class="hdr b">Interconnect</td><td></td>
  <td class="hdr mm">Memory</td><td></td>
  <td class="hdr d">DMA</td><td></td>
  <td class="hdr p">Display</td>
</tr>
<tr>
  <td class="i">instr_socket<br><span class="na">simple_initiator</span></td>
  <td class="ba">fetch &#8594;</td>
  <td class="t" rowspan="2">target_socket<br><span class="na">multi_<br>passthrough_<br>target</span></td>
  <td></td>
  <td class="t" rowspan="2">target_socket<br><span class="na">multi_<br>passthrough_<br>target</span></td>
  <td class="ba">&#8594;</td>
  <td class="t" rowspan="2">socket<br><span class="na">simple_target</span></td>
  <td></td>
  <td class="t">target_socket MMIO<br><span class="na">simple_target</span></td>
  <td></td>
  <td class="t">target_socket MMIO<br><span class="na">simple_target</span></td>
</tr>
<tr>
  <td class="i">data_socket<br><span class="na">simple_initiator</span></td>
  <td class="ba">load/store &#8594;</td>
  <td></td>
  <td></td>
  <td></td>
  <td></td>
  <td></td>
  <td></td>
  <td></td>
</tr>
<tr>
  <td></td><td></td>
  <td class="i">initiator_socket<br><span class="na">simple_initiator</span></td>
  <td class="ba">&#8594;</td>
  <td class="b" rowspan="5">regions[]<br>route by addr<br><span class="na">b_transport</span></td>
  <td class="oa">&#8594; 0x80000000</td>
  <td></td><td></td><td></td><td></td>
</tr>
<tr>
  <td></td><td></td><td></td><td></td>
  <td class="oa">&#8594; 0x10000000</td>
  <td></td>
  <td class="t">regs: SRC,DST<br>SIZE,CTRL</td>
  <td></td><td></td>
</tr>
<tr>
  <td></td><td></td><td></td><td></td>
  <td class="oa">&#8594; 0x10001000</td>
  <td></td><td></td><td></td>
  <td class="t">regs: FB_ADDR<br>W,H,CTRL</td>
</tr>
<tr>
  <td></td><td></td><td></td>
  <td class="ga">DMA.init &#8592;</td>
  <td class="ga">&#8592;</td>
  <td></td>
  <td class="i">initiator_socket<br><span class="na">simple_initiator</span></td>
  <td></td><td></td>
</tr>
<tr>
  <td></td><td></td><td></td>
  <td class="ga">Disp.init &#8592;</td>
  <td class="ga">&#8592;</td>
  <td></td><td></td><td></td>
  <td class="i">initiator_socket<br><span class="na">simple_initiator</span></td>
</tr>
</table>

<table>
<tr><td class="hdr" colspan="3">Socket Types</td></tr>
<tr>
  <td class="i">purple = simple_initiator</td>
  <td>master, calls b_transport</td>
</tr>
<tr>
  <td class="t">red = simple_target / multi_passthrough_target</td>
  <td>slave, receives b_transport</td>
</tr>
<tr><td class="hdr" colspan="3">Arrow Colors</td></tr>
<tr><td class="ba">blue arrow = DATA path</td></tr>
<tr><td class="oa">orange arrow = CONTROL path (MMIO)</td></tr>
<tr><td class="ga">green arrow = device initiator path</td></tr>
</table>
</body></html>'''

with open("docs/architecture.html","w") as f: f.write(HTML)
print("Generated docs/architecture.html")
