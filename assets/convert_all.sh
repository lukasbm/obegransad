#!/bin/bash
out_dir="../src/sprites"

python3 img2cpp.py atlas small_test.bmp 4 4 > "${out_dir}/small_test.hpp"
python3 img2cpp.py font ThinGlyphs4x6.bmp 4 6 > "${out_dir}/thin_glyphs.hpp"
python3 img2cpp.py font BoldGlyphs6x7.bmp 6 7 > "${out_dir}/bold_glyphs.hpp"
python3 img2cpp.py animation Sun.bmp 9 9 > "${out_dir}/sun.hpp"
python3 img2cpp.py animation Rain.bmp 7 8  > "${out_dir}/rain.hpp"
python3 img2cpp.py single Cloud.bmp > "${out_dir}/cloud.hpp"
python3 img2cpp.py atlas MoonSmall.bmp 9 9 > "${out_dir}/moon_small.hpp"
python3 img2cpp.py single WiFi.bmp > "${out_dir}/wifi.hpp"
python3 img2cpp.py animation Heart.bmp 9 8 > "${out_dir}/heart.hpp"
