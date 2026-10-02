#!/bin/sh
# tools/split.py with the virtual environment's Python (kept for the commands that ran it)
cd "$(dirname "$0")/.." && exec .venv/bin/python tools/split.py "$@"
