#!/usr/bin/env python3
"""Portable inference on precomputed features or one 64 Hz BVP window."""
import argparse
import json
from pathlib import Path
import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
MODEL = json.loads((HERE / "model.json").read_text(encoding="utf-8"))
FEATURES = MODEL["features_in_order"]

def infer_features(values):
    x = np.asarray(values, dtype=float)
    if x.shape != (len(FEATURES),) or not np.isfinite(x).all():
        raise ValueError("Cần đúng 14 đặc trưng hữu hạn, theo thứ tự model.json")
    z = float(MODEL["logistic_intercept"] +
              np.dot((x - np.asarray(MODEL["scaler_mean"])) /
                     np.asarray(MODEL["scaler_scale"]),
                     np.asarray(MODEL["logistic_coefficient"])))
    p = float(1.0 / (1.0 + np.exp(-z)))
    return {"stress_probability": p,
            "label": "stress" if p >= MODEL["threshold"] else "baseline"}

def main():
    ap = argparse.ArgumentParser()
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--features", help="CSV có 14 cột đặc trưng (nhiều cửa sổ)")
    group.add_argument("--bvp", help="CSV một cửa sổ BVP 3840 mẫu, 64 Hz")
    ap.add_argument("--column", default="BVP", help="Tên cột cho --bvp, mặc định BVP")
    args = ap.parse_args()
    if args.features:
        df = pd.read_csv(args.features)
        missing = [name for name in FEATURES if name not in df]
        if missing: ap.error(f"Thiếu feature: {missing}")
        for i, row in df.iterrows():
            print(json.dumps({"row": int(i), **infer_features(row[FEATURES].to_numpy(dtype=float))}))
    else:
        # Import only; never invoke the original WESAD pickle loader.
        from tao_dataset_stress_hrv_60s import extract_features
        df = pd.read_csv(args.bvp)
        if args.column not in df: ap.error(f"Thiếu cột {args.column}")
        if len(df) != 3840: ap.error(f"Cần đúng 3840 mẫu BVP ở 64 Hz, có {len(df)}")
        features, qc = extract_features(df[args.column].to_numpy(dtype=float))
        print(json.dumps({"quality": qc, **infer_features([features[k] for k in FEATURES])}))

if __name__ == "__main__": main()
