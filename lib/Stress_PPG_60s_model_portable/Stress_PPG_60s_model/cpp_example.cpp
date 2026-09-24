#include "stress_ppg_model.h"
#include <iomanip>
#include <iostream>
int main() {
    double f[stress_ppg::kFeatureCount];
    while (std::cin >> f[0]) {
        for (std::size_t i=1; i<stress_ppg::kFeatureCount; ++i)
            if (!(std::cin >> f[i])) return 1;
        auto r = stress_ppg::infer(f, stress_ppg::kFeatureCount);
        if (!r.valid) return 2;
        std::cout << std::setprecision(17) << r.stress_probability << " "
                  << (r.stress ? "stress" : "baseline") << "\n";
    }
}
