#!/bin/bash
gzip -k -9 -f -S .gz portal.html style.min.css
mv *.gz ../../data/
