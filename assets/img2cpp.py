from fire import Fire
from PIL import Image
import numpy as np
from math import log2, ceil

# converts an texture atlas or sprite sheet into a C++ array.
# output is given on stdout

def map_value(x : int, in_min=0, in_max=255, out_min=0, out_max=3) -> int:
    """
    Maps a value from one range to another.

    :param x: The input value to map.
    :param in_min: The lower bound of the input range.
    :param in_max: The upper bound of the input range.
    :param out_min: The lower bound of the output range.
    :param out_max: The upper bound of the output range.
    :return: The mapped value.
    """
    return int((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)


def convert(input_file: str, spriteWidth: int, spriteHeight: int, bits_per_pixel: int = 2, debug: bool = False) -> None:
    # load and convert to 8-bit grayscale
    img = Image.open(input_file).convert("L")
    arr = np.array(img)

    # get number of sprites
    imgHeight = arr.shape[0]
    imgWidth = arr.shape[1]
    numSpriteX = imgWidth // spriteWidth
    numSpriteY = imgHeight // spriteHeight

    if not debug:
        print("static const uint8_t sheetData[][] = {")

    for spriteY in range(numSpriteY):
        for spriteX in range(numSpriteX):
            # get sprite
            sprite = arr[
                spriteY * spriteHeight : (spriteY + 1) * spriteHeight,
                spriteX * spriteWidth : (spriteX + 1) * spriteWidth
            ]
            if debug:
                print(sprite)

            # map to 2 bits per pixel
            spriteList = sprite.ravel().tolist()
            mappedSprite = map(lambda x: map_value(x, out_max=2**bits_per_pixel-1), spriteList)
            binaryMappedSprite = map(lambda x: np.binary_repr(x, width=bits_per_pixel), mappedSprite)
            binarySpriteString = "".join(binaryMappedSprite)
            binaryArray = np.array([int(x) for x in binarySpriteString], dtype=np.uint8)
            packedArray = np.packbits(binaryArray)
            if debug:
                print(packedArray)
            hexSprite = np.array2string(packedArray, separator=", ", formatter={'int': lambda x: f"0x{x:02x}"})
            hexSpritePost = "\t" + hexSprite.replace("[", "{").replace("]", "}") + f", // {spriteY}"
            print(hexSpritePost)
            if debug:
                print("===")
    if not debug:
        print("};")

if __name__ == "__main__":
    Fire(convert)
