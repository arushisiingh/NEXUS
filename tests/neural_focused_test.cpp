#include <cmath>
#include <iostream>
#include <stdexcept>
#include "core/neural/neural_regression.hpp"
int main() { try { const auto r=nexus::neural::solve(); if(std::abs(r.initial_forward.z-.5)>1e-12 || !r.gradient_verification_passed || r.training_history.size()!=101 || r.final_loss>=r.updated_loss) throw std::runtime_error("neural verification mismatch"); for(const auto& row:r.finite_difference) if(!row.pass || row.absolute_difference>1e-5) throw std::runtime_error("gradient check failed"); std::cout<<"neural focused: PASS\n"; return 0; } catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;} }
