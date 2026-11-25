#!/usr/bin/env python3
"""Run all repository tests for the maintained K3 EFI path."""
from pathlib import Path
import sys
import unittest
root = Path(__file__).resolve().parents[1]
suite = unittest.TestSuite()
for directory in ['Platform/Spacemit/Test', 'Silicon/Spacemit/K3/Library/K3OsFdtLib/Test', 'tools/tests']:
    # Each directory is an independent, non-package test root.
    suite.addTests(unittest.TestLoader().discover(str(root/directory), pattern='test_*.py'))
sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
