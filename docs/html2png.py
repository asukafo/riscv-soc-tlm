#!/usr/bin/env python3
"""Convert architecture.html to PNG using WeasyPrint + Pillow."""
import io
from weasyprint import HTML
from PIL import Image

html_path = "docs/architecture.html"
png_path = "docs/architecture.png"

# Render HTML to PDF in memory
pdf_bytes = HTML(filename=html_path).write_pdf()

# Convert PDF to PNG via Pillow (first page only)
from PIL import Image
import subprocess, tempfile, os

# Use macOS sips to convert PDF to PNG
with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as tmp:
    tmp.write(pdf_bytes)
    tmp_path = tmp.name

subprocess.run(["sips", "-s", "format", "png", tmp_path, "--out", png_path,
                "-Z", "1800"], check=True, capture_output=True)
os.unlink(tmp_path)
print(f"Generated {png_path}")
