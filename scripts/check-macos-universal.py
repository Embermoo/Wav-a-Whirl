"""Require both CPU architectures in every bundled Mach-O binary."""
import pathlib
import subprocess
import sys
root = pathlib.Path(sys.argv[1])
checked = 0
for path in root.rglob("*"):
    if not path.is_file() or path.is_symlink():
        continue
    description = subprocess.check_output(["file", "-b", str(path)], text=True)
    if "Mach-O" not in description:
        continue
    result = subprocess.run(["lipo", str(path), "-verify_arch", "arm64", "x86_64"])
    if result.returncode:
        print(f"::error::Missing universal architecture in {path}")
        raise SystemExit(1)
    checked += 1
if not checked:
    raise SystemExit("No Mach-O binaries found")
print(f"Verified arm64 and x86_64 in all {checked} bundled Mach-O binaries.")
