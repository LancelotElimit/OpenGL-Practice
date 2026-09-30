"""Convert app.png to a multi-resolution Windows icon (requires Pillow).

Run this script after replacing app.png. The generated app.ico is checked in,
so building the editor does not require Python or Pillow.
"""

from pathlib import Path

from PIL import Image


def main():
    directory = Path(__file__).resolve().parent
    sizes = [(size, size) for size in (16, 24, 32, 48, 64, 128, 256)]
    with Image.open(directory / "app.png") as source:
        # Preserve the artwork's aspect ratio and any existing transparency.
        image = source.convert("RGBA")
        image.thumbnail((256, 256), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
        canvas.alpha_composite(image, ((256 - image.width) // 2, (256 - image.height) // 2))
        canvas.save(directory / "app.ico", format="ICO", sizes=sizes)
    with Image.open(directory / "app.ico") as icon:
        if icon.ico.sizes() != set(sizes):
            raise RuntimeError("Generated icon is missing required sizes")
    print("Generated app.ico: 16, 24, 32, 48, 64, 128, 256 pixels")


if __name__ == "__main__":
    main()
