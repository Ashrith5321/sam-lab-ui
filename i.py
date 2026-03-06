"""
Remove red-colored content from a PDF.

Converts each page to an image, identifies pixels that are
predominantly red, and replaces them with white — leaving all
other colors intact. Outputs a clean PDF.

Usage:
    python3 remove_red.py input.pdf [output.pdf]
"""

import sys
import numpy as np
from pdf2image import convert_from_path
from PIL import Image
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
import tempfile
import os


def is_red(r, g, b, a=255):
    """Determine if a pixel is 'red' using HSV-like heuristics."""
    if a < 128:
        return False
    # Red must be dominant and reasonably bright
    return (
        r > 120
        and r > g * 1.6
        and r > b * 1.6
        and (g < 140 and b < 140)
    )


def remove_red_from_image(img: Image.Image) -> Image.Image:
    """Replace red pixels with white."""
    arr = np.array(img.convert("RGBA"))
    r, g, b, a = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2], arr[:, :, 3]

    red_mask = (
        (r > 120)
        & (r > g * 1.6)
        & (r > b * 1.6)
        & (g < 140)
        & (b < 140)
        & (a >= 128)
    )

    arr[red_mask] = [255, 255, 255, 255]  # Replace with white
    return Image.fromarray(arr).convert("RGB")


def remove_red_from_pdf(input_path: str, output_path: str, dpi: int = 200):
    print(f"Converting PDF pages to images at {dpi} DPI...")
    pages = convert_from_path(input_path, dpi=dpi)

    temp_images = []
    for i, page in enumerate(pages):
        print(f"Processing page {i + 1}/{len(pages)}...")
        cleaned = remove_red_from_image(page)
        tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
        cleaned.save(tmp.name, "PNG")
        temp_images.append((tmp.name, cleaned.size))

    # Build output PDF
    print("Building output PDF...")
    first_w, first_h = temp_images[0][1]
    c = canvas.Canvas(output_path, pagesize=(first_w, first_h))

    for img_path, (w, h) in temp_images:
        c.setPageSize((w, h))
        c.drawImage(img_path, 0, 0, width=w, height=h)
        c.showPage()

    c.save()

    # Clean up temp files
    for img_path, _ in temp_images:
        os.unlink(img_path)

    print(f"Done! Saved to: {output_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 remove_red.py <input.pdf> [output.pdf]")
        sys.exit(1)

    input_pdf = sys.argv[1]
    output_pdf = sys.argv[2] if len(sys.argv) > 2 else input_pdf.rsplit(".", 1)[0] + "_no_red.pdf"

    remove_red_from_pdf(input_pdf, output_pdf)