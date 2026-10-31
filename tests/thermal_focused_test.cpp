#include <cmath>
#include <iostream>
#include <stdexcept>
#include "core/thermal/thermal_solver.hpp"
int main() { try { nexus::thermal::ThermalParameters p; const double expected[] = {1.25, 0.3125, 0.078125, 0.01953125}; std::size_t i = 0; for (const auto n : {10U,20U,40U,80U}) { const auto r = nexus::thermal::solve(p,n); if (std::abs(r.nodal_temperatures_C.front()-100)>1e-12 || std::abs(r.nodal_temperatures_C.back()-20)>1e-12 || std::abs(r.maximum_absolute_midpoint_error_C-expected[i++])>1e-10) throw std::runtime_error("thermal convergence or boundary mismatch"); } p.heat_generation_W_m3=0; const auto zero=nexus::thermal::solve(p,10); if (zero.maximum_absolute_midpoint_error_C>1e-12) throw std::runtime_error("zero heat mismatch"); std::cout<<"thermal focused: PASS\n"; return 0; } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
