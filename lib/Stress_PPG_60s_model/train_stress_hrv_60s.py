#!/usr/bin/env python3
"""Train mô hình Baseline–Stress từ đặc trưng HRV 60 giây.

Đặt file này cạnh ba file:
  stress_hrv_features_60s_train.csv
  stress_hrv_features_60s_validation.csv
  stress_hrv_features_60s_test.csv

Chạy:
  pip install numpy pandas scikit-learn matplotlib joblib
  python train_stress_hrv_60s.py

Quy trình được khóa trước khi đọc test:
  * Logistic Regression C=0.1
  * StandardScaler chỉ fit trên train
  * 14 đặc trưng HR/HRV
  * threshold xác suất = 0.5

Model evaluation chỉ học train và được dùng để báo cáo validation/test.
Sau khi đánh giá, model deployment được fit lại bằng train+validation.
"""

from __future__ import annotations

from pathlib import Path
import json

import joblib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    confusion_matrix,
    f1_score,
    precision_score,
    recall_score,
    roc_auc_score,
)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "Stress_HRV_Model"
RANDOM_SEED = 42
THRESHOLD = 0.5

FEATURES = [
    "mean_hr_bpm",
    "std_hr_bpm",
    "min_hr_bpm",
    "max_hr_bpm",
    "mean_pp_ms",
    "median_pp_ms",
    "sdnn_ms",
    "rmssd_ms",
    "sdsd_ms",
    "pnn20_pct",
    "pnn50_pct",
    "cvnn",
    "beat_count",
    "valid_rr_ratio",
]


def load_split(name: str) -> pd.DataFrame:
    path = SCRIPT_DIR / f"stress_hrv_features_60s_{name}.csv"
    if not path.exists():
        raise FileNotFoundError(f"Thiếu file: {path.name}")
    frame = pd.read_csv(path)
    required = {"window_id", "subject", "split", "label", *FEATURES}
    missing = required - set(frame.columns)
    if missing:
        raise ValueError(f"{path.name} thiếu cột: {sorted(missing)}")
    if set(frame["label"]) != {"baseline", "stress"}:
        raise ValueError(f"{path.name} phải có đúng hai nhãn baseline và stress")
    values = frame[FEATURES].to_numpy(dtype=float)
    if not np.isfinite(values).all():
        raise ValueError(f"{path.name} có NaN hoặc Inf")
    return frame


def verify_subject_split(train: pd.DataFrame, validation: pd.DataFrame, test: pd.DataFrame) -> None:
    groups = {
        "train": set(train["subject"].astype(str)),
        "validation": set(validation["subject"].astype(str)),
        "test": set(test["subject"].astype(str)),
    }
    if groups["train"] & groups["validation"]:
        raise RuntimeError("SUBJECT LEAKAGE giữa train và validation")
    if groups["train"] & groups["test"]:
        raise RuntimeError("SUBJECT LEAKAGE giữa train và test")
    if groups["validation"] & groups["test"]:
        raise RuntimeError("SUBJECT LEAKAGE giữa validation và test")
    expected = {"train": 11, "validation": 2, "test": 2}
    actual = {name: len(subjects) for name, subjects in groups.items()}
    if actual != expected:
        raise RuntimeError(f"Số subject không đúng {expected}; hiện tại {actual}")


def labels(frame: pd.DataFrame) -> np.ndarray:
    return (frame["label"] == "stress").astype(int).to_numpy()


def build_model() -> Pipeline:
    return Pipeline(
        [
            ("scaler", StandardScaler()),
            (
                "classifier",
                LogisticRegression(
                    C=0.1,
                    max_iter=5000,
                    random_state=RANDOM_SEED,
                ),
            ),
        ]
    )


def evaluate(model: Pipeline, frame: pd.DataFrame, split_name: str) -> tuple[dict, pd.DataFrame]:
    y_true = labels(frame)
    probability = model.predict_proba(frame[FEATURES])[:, 1]
    prediction = (probability >= THRESHOLD).astype(int)
    cm = confusion_matrix(y_true, prediction, labels=[0, 1])
    metrics = {
        "split": split_name,
        "windows": int(len(frame)),
        "subjects": int(frame["subject"].nunique()),
        "accuracy": float(accuracy_score(y_true, prediction)),
        "balanced_accuracy": float(balanced_accuracy_score(y_true, prediction)),
        "precision_stress": float(precision_score(y_true, prediction, zero_division=0)),
        "recall_stress": float(recall_score(y_true, prediction, zero_division=0)),
        "f1_stress": float(f1_score(y_true, prediction, zero_division=0)),
        "auc": float(roc_auc_score(y_true, probability)),
        "tn": int(cm[0, 0]),
        "fp": int(cm[0, 1]),
        "fn": int(cm[1, 0]),
        "tp": int(cm[1, 1]),
    }
    predictions = frame[["window_id", "subject", "split", "label"]].copy()
    predictions["true_code"] = y_true
    predictions["stress_probability"] = probability
    predictions["predicted_label"] = np.where(prediction == 1, "stress", "baseline")
    predictions["correct"] = predictions["label"] == predictions["predicted_label"]
    return metrics, predictions


def plot_confusion(metrics: dict, filename: str) -> None:
    matrix = np.array(
        [[metrics["tn"], metrics["fp"]], [metrics["fn"], metrics["tp"]]],
        dtype=int,
    )
    fig, ax = plt.subplots(figsize=(5.2, 4.3))
    image = ax.imshow(matrix, cmap="Blues")
    ax.set_xticks([0, 1], labels=["Baseline", "Stress"])
    ax.set_yticks([0, 1], labels=["Baseline", "Stress"])
    ax.set_xlabel("Dự đoán")
    ax.set_ylabel("Nhãn thật")
    ax.set_title(f"Confusion matrix – {metrics['split']}")
    for row in range(2):
        for col in range(2):
            ax.text(col, row, str(matrix[row, col]), ha="center", va="center", fontsize=14)
    fig.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / filename, dpi=180)
    plt.close(fig)


def export_deployment_parameters(model: Pipeline) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier: LogisticRegression = model.named_steps["classifier"]
    table = pd.DataFrame(
        {
            "feature": FEATURES,
            "scaler_mean": scaler.mean_,
            "scaler_scale": scaler.scale_,
            "logistic_coefficient": classifier.coef_[0],
        }
    )
    table.to_csv(OUTPUT_DIR / "stress_hrv_deployment_parameters.csv", index=False)
    payload = {
        "class_0": "baseline",
        "class_1": "stress",
        "threshold": THRESHOLD,
        "features_in_order": FEATURES,
        "scaler_mean": scaler.mean_.tolist(),
        "scaler_scale": scaler.scale_.tolist(),
        "logistic_coefficient": classifier.coef_[0].tolist(),
        "logistic_intercept": float(classifier.intercept_[0]),
    }
    with (OUTPUT_DIR / "stress_hrv_deployment_parameters.json").open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2)


def main() -> None:
    train = load_split("train")
    validation = load_split("validation")
    test = load_split("test")
    verify_subject_split(train, validation, test)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Model dùng để đánh giá: tuyệt đối không học validation hay test.
    evaluation_model = build_model()
    evaluation_model.fit(train[FEATURES], labels(train))
    validation_metrics, validation_predictions = evaluate(
        evaluation_model, validation, "validation"
    )
    test_metrics, test_predictions = evaluate(evaluation_model, test, "test")
    joblib.dump(evaluation_model, OUTPUT_DIR / "stress_hrv_model_evaluation.joblib")

    pd.DataFrame([validation_metrics, test_metrics]).to_csv(
        OUTPUT_DIR / "stress_hrv_metrics.csv", index=False
    )
    validation_predictions.to_csv(
        OUTPUT_DIR / "stress_hrv_validation_predictions.csv", index=False
    )
    test_predictions.to_csv(OUTPUT_DIR / "stress_hrv_test_predictions.csv", index=False)
    plot_confusion(validation_metrics, "confusion_matrix_validation.png")
    plot_confusion(test_metrics, "confusion_matrix_test.png")

    # Model triển khai: sau khi báo cáo xong mới học thêm validation; test không được dùng.
    train_validation = pd.concat([train, validation], ignore_index=True)
    deployment_model = build_model()
    deployment_model.fit(train_validation[FEATURES], labels(train_validation))
    joblib.dump(deployment_model, OUTPUT_DIR / "stress_hrv_model_deployment.joblib")
    export_deployment_parameters(deployment_model)

    print("\n===== KẾT QUẢ STRESS HRV 60 GIÂY =====")
    for metrics in (validation_metrics, test_metrics):
        print(
            f"{metrics['split']:>10}: accuracy={metrics['accuracy']:.3%}, "
            f"F1 stress={metrics['f1_stress']:.3f}, AUC={metrics['auc']:.3f}, "
            f"CM=[[{metrics['tn']}, {metrics['fp']}], "
            f"[{metrics['fn']}, {metrics['tp']}]]"
        )
    print("SUBJECT LEAKAGE: PASS")
    print(f"Model và báo cáo: {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
