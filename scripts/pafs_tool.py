#!/usr/bin/env python3
import sys
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)
from lafs_tool import *

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Kullanim: python3 pafs_tool.py <disk_img> <source_file> <dest_name>")
        sys.exit(1)
    add_file_to_lafs(sys.argv[1], sys.argv[2], sys.argv[3])
