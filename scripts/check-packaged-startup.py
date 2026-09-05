"""Smoke-check the native packaged UI with developer Qt paths removed. No audio test."""
import os
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1]).resolve()
exe = root / ("Wav-a-Whirl.app/Contents/MacOS/Main" if sys.platform == "darwin" else "Wav-a-Whirl.sh")
env = os.environ.copy()
for key in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH",
            "QML_IMPORT_PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
    env.pop(key, None)
with tempfile.TemporaryDirectory() as home, open("packaged-startup.log", "w") as log:
    env.update(HOME=home, XDG_CONFIG_HOME=home, PATH="/usr/bin:/bin:/usr/sbin:/sbin")
    process = subprocess.Popen([str(exe)], cwd=root, env=env,
                               stdout=log, stderr=subprocess.STDOUT)
    try:
        result = process.wait(timeout=5)
        log.flush()
        details = pathlib.Path("packaged-startup.log").read_text(errors="replace")
        details = details.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
        print(f"::error::Packaged startup exited {result}: {details}")
        raise SystemExit(1)
    except subprocess.TimeoutExpired:
        print("Packaged application stayed running for five seconds.")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
