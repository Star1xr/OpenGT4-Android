"""Check preferences in isolated standalone storage and an active profile."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe, disc, build = sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix="options-profile-", dir=build) as directory:
    env = dict(os.environ, GT4_OPTIONS_SAVE_DIR=directory,
               GT4_OPTIONS_FILE=str(Path(directory) / "options.txt"))
    subprocess.run([exe, disc], env=env, check=True, timeout=60)
