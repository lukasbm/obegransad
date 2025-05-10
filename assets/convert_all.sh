#!/bin/bash
out_dir="../src/sprites"

python3 img2cpp.py sheet small_test.bmp 4 4 > "${out_dir}/small_test.hpp"
python3 img2cpp.py sheet ThinGlyphs4x6.bmp 4 6 > "${out_dir}/thin_glyphs.hpp"
python3 img2cpp.py sheet BoldGlyphs6x7.bmp 6 7 > "${out_dir}/bold_glyphs.hpp"
# python3 img2cpp.py single Sun.bmp > "${out_dir}/Sun.hpp"
# python3 img2cpp.py single Moon.bmp > "${out_dir}/Moon.hpp"
# python3 img2cpp.py sheet Rain.bmp 4 4 > "${out_dir}/Rain.hpp"  # FIXME: sprite size?
# python3 
