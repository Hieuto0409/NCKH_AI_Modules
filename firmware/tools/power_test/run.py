"""Run production upload state machine against fake WiFi and ESP-MQTT APIs."""
from pathlib import Path
import shutil, subprocess, tempfile
HERE=Path(__file__).resolve().parent
FW=HERE.parents[1]
with tempfile.TemporaryDirectory(prefix='ppg-power-') as temp:
    root=Path(temp)
    shutil.copytree(FW/'include',root/'include')
    for name in ('main.cpp','WiFi.h','mqtt_client.h'):
        shutil.copyfile(HERE/name,root/name)
    shutil.copyfile(FW/'src/network/mqtt_client.cpp',root/'production.cpp')
    (root/'include/config/network_secrets.h').write_text('#define PPGFW_WIFI_SSID "test"\n#define PPGFW_TB_HOST "test"\n#define PPGFW_TB_TOKEN "test-token"\n')
    subprocess.run(['g++','-std=c++17','-DAPP_ENABLE_MQTT=1','-Iinclude','-I.',
                    'main.cpp','production.cpp','-o','test.exe'],cwd=root,check=True)
    subprocess.run([str(root/'test.exe')],cwd=root,check=True)

    # A fresh checkout with no secrets must never energize the radio.
    (root/'include/config/network_secrets.h').write_text('')
    (root/'no_config.cpp').write_text('#include "network/mqtt_client.h"\n#include <cassert>\nint main(){ppgfw::MqttClient c;c.begin();assert(!c.publish("{}"));\nfor(unsigned t=0;t<100000;t+=100)c.loop(t,true);\nassert(!c.radioActive() && WiFi.begins==0 && c.rejected()==1);}\n')
    subprocess.run(['g++','-std=c++17','-DAPP_ENABLE_MQTT=1','-Iinclude','-I.',
                    'no_config.cpp','production.cpp','-o','no-config.exe'],cwd=root,check=True)
    subprocess.run([str(root/'no-config.exe')],cwd=root,check=True)
    print('{"unconfigured_radio_off":"PASS"}')
