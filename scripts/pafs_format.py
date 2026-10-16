#!/usr/bin/env python3
import sys
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)
from lafs_format import *

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Kullanim: python3 pafs_format.py <disk_img>")
        sys.exit(1)
    format_lafs(sys.argv[1])
