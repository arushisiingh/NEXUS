#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nexus::neural {

struct Parameters {
    double w = 0.5;
    double b = 0.0;
    double v = 1.0;
    double c = 0.0;
};

struct ForwardPass {
    double x = 1.0;
    double target = 2.0;
    double z = 0.0;
    double h = 0.0;
    double prediction = 0.0;
    double prediction_error = 0.0;
    double loss = 0.0;
};

struct Gradients {
    double dJ_dw = 0.0;
    double dJ_db = 0.0;
    double dJ_dv = 0.0;
    double dJ_dc = 0.0;
};

struct FiniteDifferenceRow {
    std::string parameter;
    double analytical_gradient = 0.0;
    double numerical_gradient = 0.0;
    double absolute_difference = 0.0;
    bool pass = false;
};

struct TrainingPoint {
    std::size_t step = 0;
    double loss = 0.0;
};

struct Result {
    double learning_rate = 0.1;
    std::size_t additional_training_steps = 100;
    Parameters initial_parameters;
    ForwardPass initial_forward;
    Gradients initial_gradients;
    Parameters updated_parameters;
    double updated_loss = 0.0;
    std::vector<FiniteDifferenceRow> finite_difference;
    bool gradient_verification_passed = false;
    std::vector<TrainingPoint> training_history;
    Parameters final_parameters;
    double final_loss = 0.0;
};

void validate(double x, double target, const Parameters& parameters, double learning_rate, std::size_t additional_steps);
ForwardPass forward(const Parameters& parameters, double x, double target);
double loss(const Parameters& parameters, double x, double target);
Gradients gradients(const Parameters& parameters, double x, double target);
Parameters updated(const Parameters& parameters, const Gradients& gradients, double learning_rate);
std::vector<FiniteDifferenceRow> finite_difference_check(const Parameters& parameters, double x, double target, double epsilon = 1e-6, double tolerance = 1e-5);
Result solve(double x = 1.0, double target = 2.0, const Parameters& initial = Parameters{}, double learning_rate = 0.1, std::size_t additional_training_steps = 100);

} // namespace nexus::neural
