#!/bin/bash
out_dir="../../data/"
mkdir -p "${out_dir}"

python3 img2bwb.py ThinGlyphs4x6.bmp 4 6 > "${out_dir}/thin_glyphs.bwb"
python3 img2bwb.py BoldGlyphs6x7.bmp 6 7 > "${out_dir}/bold_glyphs.bwb"
python3 img2bwb.py Sun.bmp 9 9 > "${out_dir}/sun.bwb"
python3 img2bwb.py Rain.bmp 7 8  > "${out_dir}/rain.bwb"
python3 img2bwb.py Cloud.bmp 9 5 > "${out_dir}/cloud.bwb"
python3 img2bwb.py MoonSmall.bmp 9 9 > "${out_dir}/moon_small.bwb"
python3 img2bwb.py WiFi.bmp 10 8 > "${out_dir}/wifi.bwb"
python3 img2bwb.py Heart.bmp 9 8 > "${out_dir}/heart.bwb"
python3 img2bwb.py Firework.bmp 9 9 > "${out_dir}/firework.bwb"
