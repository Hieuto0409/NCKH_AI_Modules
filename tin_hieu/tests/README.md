# Host smoke tests

These tests do not prove medical or hardware accuracy. They check that the
pure-C modules link, filter samples, keep overlapping windows, report timestamp
gaps, detect clean synthetic peaks, and compute guarded interval features.

From the project root:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I. biquad.c SignalProcessor.c PeakDetector.c FeatureExtractor.c \
  tests/test_signal_processor.c \
  -lm -o tests/test_signal_processor
./tests/test_signal_processor
```

Expected final line:

```text
PASS: sliding-window and filter smoke test
```

Peak/feature test:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I. PeakDetector.c FeatureExtractor.c tests/test_peak_features.c \
  -lm -o tests/test_peak_features
./tests/test_peak_features
```

Expected:

```text
PASS: causal peaks and interval features
```

The tests are deterministic software checks only. They do not validate sensor
placement, ADC scaling, physiological correctness, or clinical accuracy.
