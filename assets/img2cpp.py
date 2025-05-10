"""
converts an texture atlas or sprite sheet into a C++ array.
output is given on stdout
"""
from fire import Fire
from PIL import Image
import numpy as np
from math import log2, ceil
import os

DEBUG = os.getenv("DEBUG", False)

#############################
#==== Printing functions
#############################

data_array_name = "spriteData"

front_matter = """#pragma once

#include <Arduino.h>
#include "sprites.hpp"

"""

def cpp_dataArray1D(packedArray: np.ndarray) -> str:
    # convert to hex
    hexSprite = np.array2string(packedArray, separator=", ", formatter={'int': lambda x: f"0x{x:02x}"})
    # fix formatting
    hexSpritePost = "\t" + hexSprite.replace("[", "{").replace("]", "}")
    return f"constexpr uint8_t {data_array_name}[{len(packedArray)}] = {hexSpritePost};\n"


def cpp_dataArray2D(packedArrays: list[np.ndarray]) -> str:
    assert len(packedArrays) > 0, "packedArrays is empty"
    spriteLen = len(packedArrays[0])
    atlasLen  = len(packedArrays)
    res = f"constexpr uint8_t {data_array_name}[{atlasLen}*{spriteLen}] = {{\n"
    for packed in packedArrays:
        # convert to hex
        hexSprite = np.array2string(packed, separator=", ", formatter={'int': lambda x: f"0x{x:02x}"})
        # fix formatting
        hexSpritePost = "\t" + hexSprite.replace("[", "").replace("]", "")
        res += f"\t{hexSpritePost}, //\n"
    res += "};\n"
    return res


def cpp_textureAtlasClass(spriteWidth: int, spriteHeight: int, numSprites:int) -> str:
    return f"constexpr TextureAtlas atlas({data_array_name}, {spriteWidth}, {spriteHeight}, {numSprites});"


def cpp_spriteSheetClass(spriteWidth: int, spriteHeight: int, numSprites:int) -> str:
    return f"constexpr SpriteSheet atlas({data_array_name}, {spriteWidth}, {spriteHeight}, {numSprites});"


def cpp_fontClass(spriteWidth: int, spriteHeight: int, numSprites:int, ascii_offset: int) -> str:
    return f"constexpr FontSheet atlas({data_array_name}, {spriteWidth}, {spriteHeight}, {numSprites}, {ascii_offset});"


#############################
#==== Conversion functions
############################

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


# returns the sprite as a packed array of bytes
def convertSpriteToPacked(sprite: np.ndarray, bits_per_pixel: int) -> np.ndarray:
    # map to 2 bits per pixel
    spriteList = sprite.ravel().tolist()
    # map each pixel to desired color depth
    mappedSprite = map(lambda x: map_value(x, out_max=2**bits_per_pixel-1), spriteList)
    # binarize the pixel
    binaryMappedSprite = map(lambda x: np.binary_repr(x, width=bits_per_pixel), mappedSprite)
    # join into a long binary string
    binarySpriteString = "".join(binaryMappedSprite)
    # turn the binary string back into an array
    binaryArray = np.array([int(x) for x in binarySpriteString], dtype=np.uint8)
    # pack them into bytes
    packedArray = np.packbits(binaryArray)
    return packedArray


def convertSpriteSheetToPacked(arr, spriteHeight, spriteWidth, color_depth) -> list[np.ndarray]:
    imgHeight = arr.shape[0]
    imgWidth = arr.shape[1]

    numSpriteX = imgWidth // spriteWidth
    numSpriteY = imgHeight // spriteHeight

    packedArrays = []
    for spriteY in range(numSpriteY):
        for spriteX in range(numSpriteX):
            # get sprite
            sprite = arr[
                spriteY * spriteHeight : (spriteY + 1) * spriteHeight,
                spriteX * spriteWidth : (spriteX + 1) * spriteWidth
            ]
            if DEBUG:
                print(sprite)
            packed = convertSpriteToPacked(sprite, color_depth)
            if DEBUG:
                print(packed)
                print("===")
            packedArrays.append(packed)
    return packedArrays


def load_image(input_file: str) -> np.ndarray:
    img = Image.open(input_file).convert("L")
    return np.array(img)



#############################
#==== main functions
#############################

# for a single sprite
def parse_single_sprite(input_file: str, color_depth: int = 2) -> None:
    arr = load_image(input_file)
    imgHeight = arr.shape[0]
    imgWidth = arr.shape[1]

    if not DEBUG:
        print(front_matter)

    packed = convertSpriteToPacked(arr, color_depth)
    if DEBUG:
        print(packed)

    # print the packed array
    if not DEBUG:
        print(f"// img size: {imgWidth}x{imgHeight}")
        print(cpp_dataArray1D(packed))


# for a sprite atlas
def parse_sprite_sheet(input_file: str, spriteWidth: int, spriteHeight: int, color_depth: int = 2) -> None:
    arr = load_image(input_file)
    imgHeight = arr.shape[0]
    imgWidth = arr.shape[1]

    if not DEBUG:
        print(front_matter)

    # get packed arrays
    packedArrays = convertSpriteSheetToPacked(arr, spriteHeight, spriteWidth, color_depth)

    # print the packed arrays
    if DEBUG:
        for packed in packedArrays:
            print(packed)
            print("===")
    else:
        print(f"// sprite size: {spriteWidth}x{spriteHeight} ({imgWidth}x{imgHeight} total)")
        print(cpp_dataArray2D(packedArrays))

    numSprites = len(packedArrays)

    # add the atlas class
    if not DEBUG:
        print("// keep only ONE of the following:")
        print(cpp_textureAtlasClass(spriteWidth, spriteHeight, numSprites))
        print(cpp_spriteSheetClass(spriteWidth, spriteHeight, numSprites))
        print(cpp_fontClass(spriteWidth, spriteHeight, numSprites, 32))


if __name__ == "__main__":
    Fire({
        "single": parse_single_sprite,
        "sheet": parse_sprite_sheet,
    })
