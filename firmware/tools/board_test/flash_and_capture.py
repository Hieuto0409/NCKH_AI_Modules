"""Flash a verified artifact, then reset and capture actual board UART.
Invoke from the VS Code process task or directly. No chip-wide erase or credentials.
"""
import argparse, hashlib, json, subprocess, sys, time
from pathlib import Path
import serial

FW=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--environment',required=True,choices=['esp32-s3-devkitc-1-fixture','esp32-s3-devkitc-1','esp32-s3-devkitc-1-board-diag','esp32-s3-devkitc-1-thingsboard'])
p.add_argument('--port',default='COM6')
p.add_argument('--seconds',type=int,default=45)
p.add_argument('--capture-only',action='store_true')
p.add_argument('--manifest',type=Path)
p.add_argument('--log-dir',type=Path)
a=p.parse_args()
label='thingsboard' if a.environment.endswith('-thingsboard') else ('board-diag' if a.environment.endswith('-board-diag') else ('fixture' if a.environment.endswith('-fixture') else 'production'))
logs=a.log_dir or FW/'.pio/verification/board-2026-09-27';logs.mkdir(parents=True,exist_ok=True)
logpath=logs/(label+('-capture' if a.capture_only else '-flash')+'.log')
with logpath.open('w',encoding='utf-8') as out:
    def emit(text):
        print(text,end='',flush=True);out.write(text);out.flush()
    emit('Environment: '+a.environment+'\nPort: '+a.port+'\n')
    if not a.capture_only:
        build=FW/'.pio/build'/a.environment
        expected=json.loads((a.manifest or FW.parent/'docs/power-and-noise/evidence.json').read_text())['builds'][a.environment]['firmware_bin_sha256']
        actual=hashlib.sha256((build/'firmware.bin').read_bytes()).hexdigest()
        if actual!=expected: raise SystemExit('Binary hash differs from verified build; rebuild and revalidate first.')
        emit('Verified firmware SHA256: '+actual+'\n')
        packages=Path.home()/'.platformio/packages'
        command=[sys.executable,str(packages/'tool-esptoolpy/esptool.py'),'--chip','esp32s3',
            '--port',a.port,'--baud','460800','--before','default_reset','--after','no_reset',
            'write_flash','-z','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB',
            '0x0',str(build/'bootloader.bin'),'0x8000',str(build/'partitions.bin'),
            '0xe000',str(packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin'),
            '0x10000',str(build/'firmware.bin')]
        process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
        for line in process.stdout: emit(line)
        if process.wait()!=0: raise SystemExit('Flashing failed; see log.')
    emit('\n--- LIVE BOARD UART 921600 ---\n')
    with serial.Serial(a.port,921600,timeout=0.2) as uart:
        # Keep GPIO0 released and pulse EN. The port is already open before boot prints.
        uart.dtr=False;uart.rts=True;time.sleep(0.15);uart.reset_input_buffer();uart.rts=False
        deadline=time.monotonic()+a.seconds
        while time.monotonic()<deadline:
            data=uart.readline()
            if data:
                line=data.decode('utf-8',errors='replace');emit(line)
                if (label=='fixture' and 'OFFLINE TEST COMPLETE' in line) or (label=='board-diag' and 'BOARD DIAGNOSTICS COMPLETE' in line): break
    emit('\nCapture complete. Log: '+str(logpath)+'\n')
