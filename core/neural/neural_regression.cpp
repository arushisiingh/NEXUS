#include "core/neural/neural_regression.hpp"

#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace nexus::neural {
namespace {
bool finite(double value) { return std::isfinite(value); }
void require_finite(const Parameters& p) {
    if (!finite(p.w) || !finite(p.b) || !finite(p.v) || !finite(p.c)) throw std::invalid_argument("all neural parameters must be finite");
}
double perturb(const Parameters& p, const std::string& name, double amount) {
    Parameters changed = p;
    if (name == "w") changed.w += amount;
    else if (name == "b") changed.b += amount;
    else if (name == "v") changed.v += amount;
    else if (name == "c") changed.c += amount;
    else throw std::invalid_argument("unknown neural parameter");
    return loss(changed, 1.0, 2.0);
}
}

void validate(double x, double target, const Parameters& parameters, double learning_rate, std::size_t additional_steps) {
    if (!finite(x) || !finite(target)) throw std::invalid_argument("x and target must be finite");
    require_finite(parameters);
    if (!finite(learning_rate) || learning_rate <= 0.0) throw std::invalid_argument("learning_rate must be finite and positive");
    if (additional_steps > 100000) throw std::invalid_argument("additional_training_steps must be <= 100000");
}

ForwardPass forward(const Parameters& parameters, double x, double target) {
    if (!finite(x) || !finite(target)) throw std::invalid_argument("x and target must be finite");
    require_finite(parameters);
    ForwardPass result;
    result.x = x;
    result.target = target;
    result.z = parameters.w * x + parameters.b;
    result.h = std::tanh(result.z);
    result.prediction = parameters.v * result.h + parameters.c;
    result.prediction_error = result.prediction - target;
    result.loss = 0.5 * result.prediction_error * result.prediction_error;
    if (!finite(result.loss)) throw std::runtime_error("neural forward pass produced a non-finite loss");
    return result;
}

double loss(const Parameters& parameters, double x, double target) { return forward(parameters, x, target).loss; }

Gradients gradients(const Parameters& parameters, double x, double target) {
    const auto pass = forward(parameters, x, target);
    const double dJ_dy_hat = pass.prediction_error;
    const double dJ_dz = dJ_dy_hat * parameters.v * (1.0 - pass.h * pass.h);
    Gradients result;
    result.dJ_dw = dJ_dz * x;
    result.dJ_db = dJ_dz;
    result.dJ_dv = dJ_dy_hat * pass.h;
    result.dJ_dc = dJ_dy_hat;
    if (!finite(result.dJ_dw) || !finite(result.dJ_db) || !finite(result.dJ_dv) || !finite(result.dJ_dc)) throw std::runtime_error("neural gradient calculation produced a non-finite value");
    return result;
}

Parameters updated(const Parameters& parameters, const Gradients& gradient, double learning_rate) {
    if (!finite(learning_rate) || learning_rate <= 0.0) throw std::invalid_argument("learning_rate must be finite and positive");
    Parameters result;
    result.w = parameters.w - learning_rate * gradient.dJ_dw;
    result.b = parameters.b - learning_rate * gradient.dJ_db;
    result.v = parameters.v - learning_rate * gradient.dJ_dv;
    result.c = parameters.c - learning_rate * gradient.dJ_dc;
    require_finite(result);
    return result;
}

std::vector<FiniteDifferenceRow> finite_difference_check(const Parameters& parameters, double x, double target, double epsilon, double tolerance) {
    if (!finite(epsilon) || epsilon <= 0.0) throw std::invalid_argument("finite-difference epsilon must be finite and positive");
    const auto analytic = gradients(parameters, x, target);
    const std::vector<std::pair<std::string, double>> values{{"w", analytic.dJ_dw}, {"b", analytic.dJ_db}, {"v", analytic.dJ_dv}, {"c", analytic.dJ_dc}};
    std::vector<FiniteDifferenceRow> rows;
    for (const auto& [name, analytic_value] : values) {
        Parameters plus = parameters; Parameters minus = parameters;
        if (name == "w") { plus.w += epsilon; minus.w -= epsilon; }
        if (name == "b") { plus.b += epsilon; minus.b -= epsilon; }
        if (name == "v") { plus.v += epsilon; minus.v -= epsilon; }
        if (name == "c") { plus.c += epsilon; minus.c -= epsilon; }
        const double numeric = (loss(plus, x, target) - loss(minus, x, target)) / (2.0 * epsilon);
        const double difference = std::abs(analytic_value - numeric);
        rows.push_back({name, analytic_value, numeric, difference, finite(difference) && difference <= tolerance});
    }
    return rows;
}

Result solve(double x, double target, const Parameters& initial, double learning_rate, std::size_t additional_training_steps) {
    validate(x, target, initial, learning_rate, additional_training_steps);
    Result result;
    result.learning_rate = learning_rate;
    result.additional_training_steps = additional_training_steps;
    result.initial_parameters = initial;
    result.initial_forward = forward(initial, x, target);
    result.initial_gradients = gradients(initial, x, target);
    result.updated_parameters = updated(initial, result.initial_gradients, learning_rate);
    result.updated_loss = loss(result.updated_parameters, x, target);
    result.finite_difference = finite_difference_check(initial, x, target);
    result.gradient_verification_passed = true;
    for (const auto& row : result.finite_difference) result.gradient_verification_passed = result.gradient_verification_passed && row.pass;
    result.training_history.push_back({0, result.updated_loss});
    result.final_parameters = result.updated_parameters;
    for (std::size_t step = 1; step <= additional_training_steps; ++step) {
        const auto step_gradients = gradients(result.final_parameters, x, target);
        result.final_parameters = updated(result.final_parameters, step_gradients, learning_rate);
        result.training_history.push_back({step, loss(result.final_parameters, x, target)});
    }
    result.final_loss = loss(result.final_parameters, x, target);
    return result;
}

} // namespace nexus::neural
