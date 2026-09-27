"""Exercise production OLED strings, validity gates, and result timeout without hardware."""
from pathlib import Path
import shutil, subprocess, tempfile
HERE=Path(__file__).resolve().parent
FW=HERE.parents[1]
with tempfile.TemporaryDirectory(prefix='ppg-ui-') as temp:
    root=Path(temp)
    shutil.copytree(FW/'include',root/'include',ignore=shutil.ignore_patterns('network_secrets.h'))
    for source in (HERE/'main.cpp',FW/'src/ui/oled_frame.cpp',FW/'src/app/measurement_state_machine.cpp'):
        shutil.copyfile(source,root/source.name)
    subprocess.run(['g++','-std=c++17','-Iinclude','main.cpp','oled_frame.cpp',
                    'measurement_state_machine.cpp','-o','test.exe'],cwd=root,check=True)
    subprocess.run([str(root/'test.exe')],cwd=root,check=True)
