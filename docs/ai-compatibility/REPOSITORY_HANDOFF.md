# Bàn giao firmware vào NCKH_AI_Modules

Ngày: 27/09/2026. Người dùng yêu cầu đưa mã đã sửa vào repo
`Hieuto0409/NCKH_AI_Modules`, tách rõ firmware và phần xử lý tín hiệu.

## Nguồn và bố cục

- Nguồn: `Hieuto0409/EdgeAI-PPG-Screening`, commit
  `3f2ec90882ea28f3736e76c63514bb2a27d58162`.
- Nền repo đích trước khi thêm firmware:
  `6f584d6632f08e77a893337f93b2c63ba13fcb1a`.
- Sao chép các file đã được Git quản lý trong `firmware/`, `tin_hieu/` và
  `docs/ai-compatibility/`. Không đưa cache build, binary hoặc dữ liệu đo riêng vào.
- Mã module/demo AI ở gốc giữ nguyên. README và AGENTS bổ sung hướng dẫn.
- Các báo cáo và bằng chứng đã có được giữ nguyên để truy được commit và kết quả
  kiểm thử nguồn; các đường dẫn tới repo dự án trong đó là nguồn lịch sử.

Model pin vẫn là `7d4683581c8c01ff6878bcad31a601aff271f2a0`.
Đối chiếu trước khi chuyển cho thấy 1.390 file AI được tích hợp khớp nguồn AI;
HEAD `6f584d6` chỉ đổi tài liệu so với pin. Không thay trọng số hoặc scaler.

## Kiểm chứng

Đối chiếu Git blob của toàn bộ ba thư mục được chuyển với commit nguồn trước khi
push. Đây là lần đưa nguyên bản mã đã kiểm thử sang repo, không sửa hành vi firmware.
Bằng chứng từ bản nguồn: 40/40 native test, bốn profile build, oracle feature,
suy luận ECG thật trên host và kiểm tra bộ mã hóa JSON/binary; đọc
[REPAIR_REPORT.md](REPAIR_REPORT.md) để biết chi tiết và giới hạn.

Chưa kiểm chứng trên bo, chưa có golden ECG độc lập và chưa đối chiếu detector với
raw capture có gán nhãn. Việc chuyển repo không biến các mục này thành đã đạt.

Để chạy oracle từ repo này, có thể dùng chính module AI ở gốc làm reference:

```bash
python docs/ai-compatibility/evidence/run_checks.py --reference . --feature-oracle --output audit-results.json
python firmware/tools/host_inference/run.py --output ecg-host-results.json
python firmware/tools/serialization_test/run.py
```

Oracle feature cần NumPy; các yêu cầu build và công cụ khác xem báo cáo kiểm chứng.
