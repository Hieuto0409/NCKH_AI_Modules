#include "ai/ecg_af_model_adapter.h"
#include <iostream>
#include <iomanip>
int main() {
    std::array<float, ppgfw::config::model::kEcgAfFeatureCount> f;
    while(std::cin>>f[0]) {
        for(size_t i=1;i<f.size();++i) if(!(std::cin>>f[i])) return 2;
        const auto r=ppgfw::EcgAfModelAdapter::infer(f);
        if(!r.model_available || !r.valid || r.error) return 3;
        std::cout<<std::setprecision(9)<<r.probability_af<<' '<<r.probability_non_af<<'\n';
    }
}
