#!/usr/bin/env python3
"""Convert architecture.html to PNG via WeasyPrint + macOS sips."""
import subprocess, tempfile, os
from weasyprint import HTML

pdf = HTML(filename="docs/architecture.html").write_pdf()
with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as f:
    f.write(pdf)
    tmp = f.name

subprocess.run(["sips", "-s", "format", "png", tmp, "--out", "docs/architecture.png",
                "-Z", "1600"], check=True, capture_output=True)
os.unlink(tmp)
print("Generated docs/architecture.png")
