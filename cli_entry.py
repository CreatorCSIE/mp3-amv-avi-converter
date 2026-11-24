"""Nuitka 打包入口（CLI，python -m nuitka ... cli_entry.py）。"""
import sys

from amvconv.cli import main

if __name__ == "__main__":
    sys.exit(main())
