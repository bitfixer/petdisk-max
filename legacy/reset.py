Import("env")
import os
from platformio.builder.tools.pioupload import AutodetectUploadPort

platform = env.PioPlatform()

AutodetectUploadPort(env)
upload_port = env.subst('$UPLOAD_PORT')

reset_flags = '--before default_reset --after hard_reset'

esptool = os.path.join(platform.get_package_dir("tool-esptoolpy"), "esptool.py")
esptool_cmd = f'$PYTHONEXE "{esptool}" --port {upload_port} {reset_flags} --no-stub run' 

# Multiple actions
env.AddCustomTarget(
    name="reset",
    dependencies=None,
    actions=[
        esptool_cmd,
        "pio device monitor"
    ],
    title="Reset ESP32",
    description="Resets the ESP32 board"
)