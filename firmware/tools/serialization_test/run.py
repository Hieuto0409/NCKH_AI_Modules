"""Exercise production MQTT JSON and binary logger with fake transport only."""
from pathlib import Path
import importlib.util, json, shutil, subprocess, tempfile
FW=Path(__file__).resolve().parents[2]
def main():
    with tempfile.TemporaryDirectory(prefix='ppg-serialize-') as tmp:
        root=Path(tmp);shutil.copytree(FW/'include',root/'include')
        for name in ('Arduino.h','main.cpp'):shutil.copyfile(Path(__file__).parent/name,root/name)
        for source in ('logging/binary_logger.cpp','network/telemetry_publisher.cpp'):
            shutil.copyfile(FW/'src'/source,root/Path(source).name)
        subprocess.run(['g++','-std=c++17','-DAPP_ENABLE_BINARY_LOG=1','-DAPP_ENABLE_MQTT=0','-Iinclude','-I.',
            'main.cpp','binary_logger.cpp','telemetry_publisher.cpp','-o','test.exe'],cwd=root,check=True)
        payload=subprocess.check_output([str(root/'test.exe')],cwd=root,text=True).strip(); data=json.loads(payload)
        assert data['schema']==3 and data['ecg_window_us']==[9000000,39000000]
        assert data['ppg_window_us']==[4000000,64000000] and data['spo2_window_us']==[60000000,64000000]
        assert data['stress_label']==2 and data['rhythm_label']==4 and not data['normal']
        assert abs(data['stress_probability']-.72)<1e-6 and abs(data['af_probability']-.13)<1e-6
        assert data['spo2'] is None and not data['spo2_clinically_validated'] and len(payload)<1536
        spec=importlib.util.spec_from_file_location('convert',FW/'tools/log_convert/log_convert.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        raw=(root/'capture.bin').read_bytes(); rows=list(mod.records(raw));types={r[2]:r for r in rows}
        assert all(r[1]==2 for r in rows) and all(i in types for i in range(38,46))
        for kind,start,duration in [(38,4000000,60000000),(39,9000000,30000000),(40,60000000,4000000)]:
            assert types[kind][3:5]==(start,duration)
        assert types[41][7]>>8==4 and types[42][7]>>8==2
        spec=importlib.util.spec_from_file_location('replay',FW/'tools/replay/replay.py')
        replay=importlib.util.module_from_spec(spec);spec.loader.exec_module(replay)
        names=['sync','schema','record_type','timestamp_us','sequence','value_a','value_b','flags','checksum']
        windows=replay.window_summaries([dict(zip(names,map(str,row))) for row in rows])
        assert len(windows)==3 and windows[1]['start_us']==9000000 and windows[1]['end_us']==39000000
        assert windows[2]['quality_scope']=='ppg60_conservative'
        corrupted=bytearray(raw);corrupted[20]^=1
        try: list(mod.records(corrupted));raise AssertionError('CRC accepted corruption')
        except ValueError:pass
        print(json.dumps({'mqtt_bytes':len(payload),'binary_records':len(rows),'schema':2,'feature_schema':3,
            'checks':'window/labels/probability/null/clinical flag/binary CRC round-trip passed'},indent=2))
if __name__=='__main__':main()
