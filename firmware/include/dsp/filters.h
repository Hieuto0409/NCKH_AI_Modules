#pragma once

namespace ppgfw {

class ExponentialDcBlocker {
public:
    explicit ExponentialDcBlocker(float alpha);
    void reset();
    float update(float input);
    float dc() const;

private:
    float alpha_{};
    float dc_{};
    bool initialized_{};
};

class ExponentialLowPass {
public:
    explicit ExponentialLowPass(float alpha);
    void reset();
    float update(float input);

private:
    float alpha_{};
    float state_{};
    bool initialized_{};
};

}

