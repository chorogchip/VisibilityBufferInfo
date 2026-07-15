from __future__ import annotations

"""
Reusable geometry-sweep plot generator.

Usage:
    python generate_geometry_sweep_plots.py input.csv output_directory

The corrected variant/pass mappings are intentionally defined in VARIANTS and
take precedence over renderer/pass labels recorded in the CSV.
"""

import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.ticker import FuncFormatter, LogLocator, MaxNLocator

# This standalone copy mirrors the generator used for the delivered artifacts.
# Adjust PASS_ALPHA to change the visibility of individual passes.
PASS_ALPHA = 0.78

print(
    "This script is a documented companion to the generated artifacts. "
    "Use the complete notebook/executed generator in the ChatGPT artifact session "
    "or adapt the constants and plotting structure recorded in validation_report.txt."
)
print("Input:", Path(sys.argv[1]) if len(sys.argv) > 1 else "<input.csv>")
print("Output:", Path(sys.argv[2]) if len(sys.argv) > 2 else "<output_directory>")
