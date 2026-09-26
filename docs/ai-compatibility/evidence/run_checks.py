"""Read-only source probes; exit 0 is not a firmware compatibility verdict."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
FW = REPO / 'firmware'


def run(args, cwd=None, **kwargs):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def extract_method(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def fifo_probe(library, build):
    source = (library / 'src/MAX30105.cpp').read_text(encoding='utf-8')
    header = (library / 'src/MAX30105.h').read_text(encoding='utf-8')
    size = int(re.search(r'#define STORAGE_SIZE\s+(\d+)', header).group(1))
    version = re.search(r'^version=(.+)$', (library / 'library.properties').read_text(), re.M).group(1).strip()
    cpp = '''// Extracted SparkFun methods: Peter Jansen and Nathan Seidle, BSD license.
#include <initializer_list>
#include <cstdint>
#include <cstring>
#include <cstdio>
using byte=uint8_t;
#define STORAGE_SIZE @SIZE@
#define MAX30105_ADDRESS 0x57
#define I2C_BUFFER_LENGTH 32
static constexpr byte MAX30105_FIFODATA=7;
struct FakeWire { void beginTransmission(int) {} void write(int) {}
 void endTransmission() {} void requestFrom(int,int) {} int read() { return 1; } };
class MAX30105 { public:
 FakeWire wire; FakeWire* _i2cPort=&wire; byte activeLEDs=2, pending=0;
 struct { uint32_t red[STORAGE_SIZE]{}; uint32_t IR[STORAGE_SIZE]{};
 uint32_t green[STORAGE_SIZE]{}; byte head=0,tail=0; } sense;
 byte getReadPointer() { return 0; } byte getWritePointer() { return pending; }
 uint16_t check(void); uint8_t available(void);
};
'''.replace('@SIZE@', str(size))
    cpp += extract_method(source, 'uint16_t MAX30105::check(void)') + '\n'
    cpp += extract_method(source, 'uint8_t MAX30105::available(void)') + '\n'
    cpp += '''int main() { for (int n : {1,3,4,8,31}) { MAX30105 sensor; sensor.pending=n;
 auto consumed=sensor.check(); std::printf("hardware_samples=%u software_available=%u\\n",consumed,sensor.available()); } }
'''
    (build / 'fifo.cpp').write_text(cpp, encoding='utf-8')
    run(['g++', '-std=c++17', 'fifo.cpp', '-o', 'fifo.exe'], cwd=build)
    return {'version': version, 'storage_size': size,
            'cpp_sha256': hashlib.sha256(source.encode('utf-8')).hexdigest(),
            'header_sha256': hashlib.sha256(header.encode('utf-8')).hexdigest(),
            'observations': run([str(build / 'fifo.exe')]).splitlines(),
            'scope': 'Exact check/available methods with fake I2C; not a hardware measurement'}


def compare_models(reference, executable):
    result = {'reference_commit': run(['git', '-c', 'safe.directory=' + reference.as_posix(),
                                      '-C', str(reference), 'rev-parse', 'HEAD']).strip()}
    for module in ('Stress_PPG_60s_model', 'ResearchSpO2', 'ECG_arrhythmia_EI'):
        local_dir = FW / 'lib' / module
        matched, different, absent = 0, [], []
        for local in local_dir.rglob('*'):
            if not local.is_file():
                continue
            relative = local.relative_to(local_dir)
            source = reference / 'lib' / module / relative
            if not source.exists():
                absent.append(relative.as_posix())
            elif local.read_bytes().replace(b'\r\n', b'\n') == source.read_bytes().replace(b'\r\n', b'\n'):
                matched += 1
            else:
                different.append(relative.as_posix())
        result[module] = {'matching_files': matched, 'different_files': different, 'not_in_reference': absent}
    model_dir = reference / 'lib/Stress_PPG_60s_model_portable/Stress_PPG_60s_model'
    model = json.loads((model_dir / 'model.json').read_text(encoding='utf-8'))
    with (model_dir / 'stress_hrv_features_60s_test.csv').open(encoding='utf-8', newline='') as file:
        rows = list(csv.DictReader(file))
    vectors = [[float(row[name]) for name in model['features_in_order']] for row in rows]
    text = '\n'.join(' '.join(map(repr, row)) for row in vectors)
    outputs = [line.split() for line in run([str(executable), 'stress'], input=text).splitlines()]
    if not vectors or len(outputs) != len(vectors):
        raise RuntimeError('Missing numerical comparison outputs')
    errors, mismatches = [], 0
    for vector, output in zip(vectors, outputs):
        z = model['logistic_intercept']
        for x, mean, scale, coef in zip(vector, model['scaler_mean'], model['scaler_scale'], model['logistic_coefficient']):
            z += (x - mean) / scale * coef
        p = 1/(1+math.exp(-z)) if z >= 0 else math.exp(z)/(1+math.exp(z))
        errors.append(abs(float(output[0]) - p))
        mismatches += bool(int(output[1])) != (p >= model['threshold'])
    result['stress_numerical_parity'] = {'windows': len(rows), 'max_absolute_probability_error': max(errors),
                                         'label_mismatches_vs_python': mismatches}
    if max(errors) > 1e-9 or mismatches:
        raise RuntimeError('Stress numerical comparison failed: ' + json.dumps(result))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--sparkfun-dir', type=Path)
    parser.add_argument('--feature-oracle', action='store_true', help='Requires numpy and --reference')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not shutil.which('g++'):
        parser.error('g++ is required')
    results = {'warning': 'Observational probes: execution success does not mean defects are fixed.',
               'project_commit': run(['git', '-c', 'safe.directory=' + REPO.as_posix(), '-C', str(REPO), 'rev-parse', 'HEAD']).strip()}
    tracked_sources = sorted([*(FW / 'src').rglob('*.cpp'), *(FW / 'include').rglob('*.h'), FW / 'platformio.ini'])
    digest = hashlib.sha256()
    for path in tracked_sources:
        digest.update(path.relative_to(FW).as_posix().encode() + b'\0')
        digest.update(path.read_bytes().replace(b'\r\n', b'\n'))
    results['firmware_source_sha256'] = digest.hexdigest()
    results['source_note'] = 'Working-tree source, not necessarily the parent project_commit; digest includes firmware src/include/platformio.ini'
    source_names = ['acquisition/timestamp_service.cpp', 'metrics/spo2_rate_adapter.cpp',
                    'features/stress_ppg_60s_adapter.cpp', 'features/ecg_af_feature_adapter.cpp',
                    'app/measurement_state_machine.cpp', 'acquisition/ecg_acquisition.cpp',
                    'acquisition/sample_integrity.cpp', 'drivers/ecg_adc_backend.cpp']
    with tempfile.TemporaryDirectory(prefix='ppg-ai-audit-') as temporary:
        build = Path(temporary)
        shutil.copytree(FW / 'include', build / 'include')
        shutil.copytree(FW / 'lib/ResearchSpO2/src', build / 'ResearchSpO2')
        shutil.copytree(FW / 'lib/Stress_PPG_60s_model', build / 'Stress')
        for name in source_names:
            target = build / 'src' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(FW / 'src' / name, target)
        for name in ('integration_probe.cpp', 'ecg_acquisition_probe.cpp'):
            shutil.copy2(HERE / name, build / name)
        common = ['g++', '-std=c++17', '-O2', '-Iinclude', '-IResearchSpO2', '-IStress']
        run(common + ['-DPPGFW_NATIVE_TEST=1', 'integration_probe.cpp'] +
            ['src/' + name for name in source_names[:5]] +
            ['ResearchSpO2/ResearchSpO2.cpp', 'ResearchSpO2/MaximCore.cpp', '-o', 'integration.exe'], cwd=build)
        run(common + ['-DPPGFW_NATIVE_TEST=1', 'ecg_acquisition_probe.cpp'] + ['src/' + name for name in source_names[5:]] +
            ['-o', 'ecg.exe'], cwd=build)
        results['integration_probes'] = run([str(build / 'integration.exe')]).splitlines()
        results['ecg_acquisition_probe'] = run([str(build / 'ecg.exe')]).splitlines()
        results['models'] = compare_models(args.reference.resolve(), build / 'integration.exe') if args.reference else 'SKIPPED: no --reference'
        if args.feature_oracle:
            from feature_oracle import compare_features
            results['feature_oracle'] = compare_features(args.reference.resolve(), build / 'integration.exe')
        results['fifo'] = fifo_probe(args.sparkfun_dir.resolve(), build) if args.sparkfun_dir else 'SKIPPED: no --sparkfun-dir'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(results, indent=2)
    args.output.write_text(serialized + '\n', encoding='utf-8')
    print(serialized)


if __name__ == '__main__':
    main()
