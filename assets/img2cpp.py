"""
converts an texture atlas or sprite sheet into a C++ array.
output is given on stdout
"""
from fire import Fire
from PIL import Image
import numpy as np
import os

DEBUG = os.getenv("DEBUG", False)

#############################
#==== Printing functions
#############################

FRONT_MATTER = """#pragma once

#include <Arduino.h>
#include "sprites.hpp"
"""

def cpp_dataArray1D(packedArray: np.ndarray) -> str:
    # convert to hex
    hexSprite = np.array2string(packedArray, separator=", ", max_line_width=200, formatter={'int': lambda x: f"0x{x:02x}"})
    # fix formatting
    hexSpritePost = hexSprite.replace("[", "{").replace("]", "}")
    return f"static constexpr uint8_t data[{len(packedArray)}] = {hexSpritePost};\n"


def cpp_dataArray2D(packedArrays: list[np.ndarray]) -> str:
    assert len(packedArrays) > 0, "packedArrays is empty"
    spriteLen = len(packedArrays[0])
    atlasLen  = len(packedArrays)
    res = f"static constexpr uint8_t data[{atlasLen} * {spriteLen}] = {{\n"
    for packed in packedArrays:
        # convert to hex
        hexSprite = np.array2string(packed, separator=", ", max_line_width=200, formatter={'int': lambda x: f"0x{x:02x}"})
        # fix formatting
        hexSpritePost = hexSprite.replace("[", "").replace("]", "")
        res += f"    {hexSpritePost}, //\n"
    res += "};\n"
    return res


def SingleSprite(img_path: str, class_name: str, color_depth : int = 2):
    img = load_image(img_path)
    spriteHeight = img.shape[0]
    spriteWidth = img.shape[1]
    packed = convertSpriteToPacked(img, color_depth)

    if not DEBUG:
        print(FRONT_MATTER)

    if DEBUG:
        print(packed)
    else:
        print(f"// image size: {spriteWidth}x{spriteHeight}")
        print(cpp_dataArray1D(packed))
    
    if not DEBUG:
        print(f"struct {class_name} : SingleSprite")
        print("{")
        print(f"    constexpr {class_name}() : SingleSprite(data, {spriteWidth}, {spriteHeight}) {{}}")
        print("};")


def TextureAtlas(img_path: str, class_name: str,  spriteWidth: int, spriteHeight: int, color_depth: int = 2):
    img = load_image(img_path)
    imgHeight = img.shape[0]
    imgWidth = img.shape[1]
    packed = convertSpriteSheetToPacked(img, spriteHeight, spriteWidth, color_depth)
    assert len(packed) > 0
    spriteBytes = len(packed[0])
    spriteCount  = len(packed)

    if not DEBUG:
        print(FRONT_MATTER)
    
    # data
    if DEBUG:
        for p in packed:
            print(p)
            print("===")
    else:
        print(f"// sprite size: {spriteWidth}x{spriteHeight} (total: {imgWidth}x{imgHeight})")
        print(cpp_dataArray2D(packed))
    
    # wrapper
    if not DEBUG:
        print(f"struct {class_name} : TextureAtlas")
        print("{")
        print(f"    constexpr {class_name}() : TextureAtlas(data, {spriteWidth}, {spriteHeight}, {spriteCount}, {spriteBytes}) {{}}")
        print("};")


def AnimationSheet(img_path: str, class_name: str, spriteWidth: int, spriteHeight: int, color_depth: int = 2):
    img = load_image(img_path)
    imgHeight = img.shape[0]
    imgWidth = img.shape[1]
    packed = convertSpriteSheetToPacked(img, spriteHeight, spriteWidth, color_depth)
    assert len(packed) > 0
    spriteBytes = len(packed[0])
    spriteCount  = len(packed)

    if not DEBUG:
        print(FRONT_MATTER)
    
    # data
    if DEBUG:
        for p in packed:
            print(p)
            print("===")
    else:
        print(f"// sprite size: {spriteWidth}x{spriteHeight} (total: {imgWidth}x{imgHeight})")
        print(cpp_dataArray2D(packed))
    
    # wrapper
    if not DEBUG:
        print(f"struct {class_name} : AnimationSheet")
        print("{")
        print(f"    constexpr {class_name}() : AnimationSheet(data, {spriteWidth}, {spriteHeight}, {spriteCount}, {spriteBytes}) {{}}")
        print("};")


def FontSheet(img_path: str, class_name: str, spriteWidth: int, spriteHeight: int, ascii_offset: int = 32, color_depth: int = 2):
    img = load_image(img_path)
    imgHeight = img.shape[0]
    imgWidth = img.shape[1]
    packed = convertSpriteSheetToPacked(img, spriteHeight, spriteWidth, color_depth)
    assert len(packed) > 0
    spriteBytes = len(packed[0])
    spriteCount  = len(packed)

    if not DEBUG:
        print(FRONT_MATTER)
    
    # data
    if DEBUG:
        for p in packed:
            print(p)
            print("===")
    else:
        print(f"// sprite size: {spriteWidth}x{spriteHeight} (total: {imgWidth}x{imgHeight})")
        print(cpp_dataArray2D(packed))
    
    # wrapper
    if not DEBUG:
        print(f"struct {class_name} : FontSheet")
        print("{")
        print(f"    constexpr {class_name}() : FontSheet(data, {spriteWidth}, {spriteHeight}, {spriteCount}, {spriteBytes}, {ascii_offset}) {{}}")
        print("};")

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
    # map/downscale/dither the value.
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

if __name__ == "__main__":
    Fire({
        "single": SingleSprite,
        "atlas": TextureAtlas,
        "animation": AnimationSheet,
        "font": FontSheet
    })
