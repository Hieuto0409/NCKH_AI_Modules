"""Compare production adapters to numerical expressions extracted from the pinned Python bridge.
Requires NumPy only; does not run the bridge's different waveform detector.
"""
import ast, hashlib, subprocess
import numpy as np

def compare_features(reference, exe):
    source=(reference/'tools/offline_data_bridge.py').read_text(encoding='utf-8')
    tree=ast.parse(source)
    fn=next(n for n in ast.walk(tree) if isinstance(n,ast.FunctionDef) and n.name=='calc_9')
    scope={'np':np}; exec(compile(ast.Module(body=[fn],type_ignores=[]),'<pinned calc_9>','exec'),scope)
    ecg_vectors=[[800,1000,900,1100],[800,850,900,950],[200,250,1999,2000],
                 [1000,1000,1000],[750,750.001,799.999,800.001,900]]
    rng=np.random.default_rng(20260926)
    ecg_vectors += [rng.integers(300,1800,size=n).tolist() for n in [3,4,17,70,150]]
    def cpp(mode,vectors):
        text='\n'.join(str(len(v))+' '+' '.join(map(str,v)) for v in vectors)+'\n'
        r=subprocess.run([str(exe),mode],input=text,text=True,capture_output=True,check=True)
        rows=[list(map(float,line.split())) for line in r.stdout.splitlines()]
        assert len(rows)==len(vectors) and all(r[0]==1 for r in rows)
        return [np.array(r[1:]) for r in rows]
    ecg_err=[]
    for v,actual in zip(ecg_vectors,cpp('ecg_features',ecg_vectors)):
        # The production API takes float milliseconds. Compare identical inputs.
        rr=np.asarray(v,dtype=np.float32).astype(np.float64)/1000
        expected=np.array(list(scope['calc_9'](rr,np.diff(rr)).values()))
        err=float(np.max(np.abs(expected-actual))); ecg_err.append(err)
        assert np.allclose(actual,expected,atol=1e-5,rtol=1e-6),(v,actual,expected)
    feats=next(n for n in ast.walk(tree) if isinstance(n,ast.Assign) and
        any(isinstance(t,ast.Name) and t.id=='feats' for t in n.targets))
    expression=compile(ast.Expression(body=feats.value),'<pinned stress feats>','eval')
    peak_vectors=[]
    for intervals in [[1000000]*61,[800000,840000,960000,880000]*20,[640000,680000,720000,760000]*25]:
        peak_vectors.append([int(x) for x in np.cumsum([0]+intervals) if x<60000000])
    stress_err=[]
    for peaks,actual in zip(peak_vectors,cpp('stress_peaks',peak_vectors)):
        raw=np.diff(peaks)/1000000
        phys=raw[(raw>=60/180)&(raw<=60/40)]
        med=float(np.median(phys)); rr=phys[(phys>=med*.70)&(phys<=med*1.30)]
        ms=rr*1000
        env={'np':np,'rr_sec':rr,'rr_ms':ms,'diff_ms':np.diff(ms),'hr':60/rr,
             'mean_pp':float(np.mean(ms)),'peaks':peaks,'valid_ratio':len(rr)/len(raw)}
        expected=np.array(list(eval(expression,env).values()))
        err=float(np.max(np.abs(expected-actual))); stress_err.append(err)
        assert np.allclose(actual,expected,atol=1e-9,rtol=1e-9),(actual,expected)
    return {'oracle':'AST-extracted calc_9 and Stress feats expressions from offline_data_bridge.py; identical intervals only',
            'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'numpy':np.__version__,
            'ecg_cases':len(ecg_vectors),'ecg_max_absolute_error':max(ecg_err),
            'stress_cases':len(peak_vectors),'stress_max_absolute_error':max(stress_err),
            'boundary_policy':'Strict > thresholds preserved; ECG inputs rounded to API float32 before Python comparison'}
