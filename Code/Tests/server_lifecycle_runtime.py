#!/usr/bin/env python3
"""[TEMP] Demo announcements and spawn teardown over strict dedicated engine objects.

Use --engine-build <completed CMake build> --output <separate fixture directory>.
No production startup, hook process, socket, game content or window is exercised.
"""
from pathlib import Path
import sys
from sticky_projectile_runtime import main

if __name__ == "__main__":
    sys.exit(main(Path(__file__).with_suffix(".cpp")))
