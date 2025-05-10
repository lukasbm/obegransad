#!/bin/bash
out_dir="../src/sprites"

python3 img2cpp.py sheet small_test.bmp 4 4 > "${out_dir}/small_test.h"
python3 img2cpp.py sheet ThinGlyphs4x6.bmp 4 6 > "${out_dir}/ThingGlyphs.h"
python3 img2cpp.py sheet BoldGlyphs6x7.bmp 6 7 > "${out_dir}/BoldGlyphs.h"
# python3 img2cpp.py single Sun.bmp > "${out_dir}/Sun.h"
# python3 img2cpp.py single Moon.bmp > "${out_dir}/Moon.h"
# python3 img2cpp.py sheet Rain.bmp 4 4 > "${out_dir}/Rain.h"  # FIXME: sprite size?
# python3 
