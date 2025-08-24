from fire import Fire
from PIL import Image
import numpy as np
from pathlib import Path
import sys

def load_image(input_file: str) -> np.ndarray:
    img = Image.open(input_file).convert("L")
    return np.array(img, dtype=np.uint8)

def main(input_file: str, sprite_width: int, sprite_height: int):
    """
    Converts an input image to grayscale, flattens it, and writes the pixel data along with
    sprite dimensions as a header to a .bwb binary file.
    
    Args:
        input_file (str): Path to the input image file.
        sprite_width (int): Width of the sprite.
        sprite_height (int): Height of the sprite.
    """
    assert sprite_width <= 255, "Sprite width must be <= 255"
    assert sprite_height <= 255, "Sprite height must be <= 255"
    assert Path(input_file).exists(), f"Input file {input_file} does not exist"

    img = load_image(input_file)
    pixels = img.ravel().tolist()

    sys.stdout.buffer.write(bytearray([sprite_width, sprite_height]))
    sys.stdout.buffer.write(bytearray(pixels))
    sys.stdout.buffer.flush()
    
    # with open(Path(input_file).stem + ".bwb", "wb+") as f:
    #     # write header
    #     f.write(bytearray([sprite_width, sprite_height]))
    #     # write file content
    #     f.write(bytearray(pixels))


if __name__ == "__main__":
    Fire(main)
