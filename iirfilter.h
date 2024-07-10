#include <vector>

/*
std::vector<std::vector<double>> sos = {    // 2nd order butterworth filter
    {0.02008337, 0.04016673, 0.02008337, 1.0, -1.59934348, 0.71529944},
    {1.0, -2.0, 1.0, 1.0, -1.88321188, 0.89661966}
};*/

/*
std::vector<std::vector<double>> sos = {    // 3rd order elliptic filter
    { 0.04966687,  0.,         -0.04966687,  1.,         -1.73794489,  0.77710116},
    { 1.,         -1.6871129,   1.,          1.,         -1.72356976,  0.89911169},
    { 1.,         -1.9941593,   1.,          1.,         -1.96469015,  0.97477867}
};*/

/*
std::vector<std::vector<double>> sos = {    // 3rd order butterworth filter
    {0.00289819, 0.00579639, 0.00289819, 1.00000000, -1.68850026, 0.72654253},
    {1.00000000, 0.00000000, -1.00000000, 1.00000000, -1.64554221, 0.78455539},
    {1.00000000, -2.00000000, 1.00000000, 1.00000000, -1.92164062, 0.93344451}
};*/

std::vector<std::vector<double>> sos = {    // 3rd order butterworth with band 0.5 - 1.5 (30 - 90 Hz)
    {0.00094131, 0.00188263, 0.00094131, 1.00000000, -1.77998686, 0.80978403},
    {1.00000000, 0.00000000, -1.00000000, 1.00000000, -1.77705266, 0.85912856},
    {1.00000000, -2.00000000, 1.00000000, 1.00000000, -1.93291941, 0.94475576}
};


struct SOSState {
    double w1 = 0.0, w2 = 0.0;  // Past input values
};


inline double apply_sos_section(double input, const std::vector<double>& coeffs, SOSState& state) {
    double w0 = input - coeffs[4] * state.w1 - coeffs[5] * state.w2;
    double output = coeffs[0] * w0 + coeffs[1] * state.w1 + coeffs[2] * state.w2;
    state.w2 = state.w1;
    state.w1 = w0;
    return output;
}
