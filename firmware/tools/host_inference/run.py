"""Execute the actual pinned ECG model on the host, not the native-test stub."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile
from concurrent.futures import ThreadPoolExecutor
FW=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    vectors=[[1,1,0,0,0,0,0,1,1],[.95,.95,.129099445,.173205081,100,.13589415,.15,.8,1.1],
             [.7,.65,.25,.35,80,.35714286,.3,.3,1.4]]
    with tempfile.TemporaryDirectory(prefix='ppg-ecg-host-') as tmp:
        root=Path(tmp); shutil.copytree(FW/'lib/ECG_arrhythmia_EI',root/'model')
        shutil.copytree(FW/'include',root/'include')
        for source,name in [(FW/'src/ai/ecg_af_model_adapter.cpp','adapter.cpp'),
                            (Path(__file__).parent/'main.cpp','main.cpp'),(Path(__file__).parent/'port.cpp','port.cpp')]:
            shutil.copyfile(source,root/name)
        cmd=['g++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DEIDSP_USE_CMSIS_DSP=0','-DEI_PORTING_POSIX=0',
             '-DEI_PORTING_MINGW32=0','-Iinclude','-Imodel','main.cpp','adapter.cpp','port.cpp',
             'model/tflite-model/tflite_learn_1119067_4_compiled.cpp','-o','infer.exe']
        sdk=root/'model/edge-impulse-sdk'
        sources=[p.relative_to(root).as_posix() for p in (sdk/'tensorflow/lite').rglob('*.cc')
                 if not any(x in p.name for x in ('test','mock','kernel_runner'))]
        sources += [p.relative_to(root).as_posix() for p in (sdk/'dsp/kissfft').glob('*.cpp')]
        sources += ['model/edge-impulse-sdk/tensorflow/lite/c/common.c']
        sources += ['main.cpp','adapter.cpp','port.cpp','model/tflite-model/tflite_learn_1119067_4_compiled.cpp']
        flags=cmd[1:cmd.index('main.cpp')]
        objects=[]
        def compile_one(item):
            index,source=item; obj=f'object{index}.o'
            subprocess.run(['g++',*flags,'-c',source,'-o',obj],cwd=root,check=True)
            return obj
        with ThreadPoolExecutor(max_workers=4) as pool:
            objects=list(pool.map(compile_one,enumerate(sources)))
        (root/'link.rsp').write_text('\n'.join(['-Wl,--gc-sections',*objects,'-o','infer.exe']))
        subprocess.run(['g++','@link.rsp'],cwd=root,check=True)
        inputs='\n'.join(' '.join(map(str,v)) for v in vectors)+'\n'
        completed=subprocess.run([str(root/'infer.exe')],input=inputs,text=True,capture_output=True,check=True)
        outputs=[list(map(float,line.split())) for line in completed.stdout.splitlines()]
        assert len(outputs)==len(vectors)
        for p in outputs:
            assert all(0<=x<=1 for x in p) and abs(sum(p)-1)<=1/256+1e-7, p
        result={'scope':'Synthetic HRV vectors; actual exported ECG inference; no clinical accuracy or independent golden oracle claim',
                'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],
                'model_sha256':hashlib.sha256((root/'model/tflite-model/tflite_learn_1119067_4_compiled.cpp').read_bytes().replace(b'\r\n',b'\n')).hexdigest(),
                'output_quantization_step':1/256, 'probability_sum_tolerance':1/256+1e-7,
                'labels':['AF','non-AF'],'vectors':vectors,'probabilities':outputs}
        args.output.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
if __name__=='__main__': main()
