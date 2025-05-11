#!/bin/bash
out_dir="../src/sprites"

# python3 img2cpp.py sheet small_test.bmp 4 4 > "${out_dir}/small_test.hpp"
# python3 img2cpp.py sheet ThinGlyphs4x6.bmp 4 6 > "${out_dir}/thin_glyphs.hpp"
# python3 img2cpp.py sheet BoldGlyphs6x7.bmp 6 7 > "${out_dir}/bold_glyphs.hpp"

python3 img2cpp.py sheet Sun.bmp 9 9 > "${out_dir}/sun.hpp"
# python3 img2cpp.py single Moon.bmp > "${out_dir}/moon.hpp"
python3 img2cpp.py sheet Rain.bmp 7 8  > "${out_dir}/rain.hpp"
python3 img2cpp.py single Cloud.bmp > "${out_dir}/cloud.hpp"
