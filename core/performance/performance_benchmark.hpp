#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/thermal/thermal_solver.hpp"

namespace nexus::performance {

struct BenchmarkConfig {
    std::size_t case_count = 240;
    std::size_t mesh_size = 40;
    std::uint64_t seed = 20241031;
    std::vector<std::size_t> worker_counts{1, 2, 4};
};

struct WorkloadCase {
    std::size_t case_id = 0;
    thermal::ThermalParameters parameters;
};

struct Workload {
    std::uint64_t seed = 0;
    std::size_t mesh_size = 0;
    std::vector<WorkloadCase> cases;
};

struct ExecutionResult {
    std::vector<thermal::ThermalResult> case_results;
    double elapsed_ms = 0.0;
};

struct AgreementResult {
    double maximum_absolute_temperature_difference_C = 0.0;
    bool agreement = true;
    std::size_t cases_compared = 0;
    std::vector<std::size_t> failed_case_ids;
};

struct SystemInfo {
    std::string cpu_model;
    std::size_t physical_core_count = 1;
    std::size_t logical_core_count = 1;
    std::uint64_t total_ram_bytes = 0;
    std::string compiler;
    std::string optimization_flags;
};

struct WorkerTiming {
    std::size_t worker_count = 1;
    std::vector<double> measured_runs_ms;
    double median_ms = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

struct BenchmarkResult {
    BenchmarkConfig config;
    Workload workload;
    double launch_to_ready_ms = 0.0;
    double workload_generation_ms = 0.0;
    double serial_median_ms = 0.0;
    std::vector<double> serial_measured_runs_ms;
    std::vector<WorkerTiming> parallel_timings;
    AgreementResult numerical_agreement;
    SystemInfo system;
};

std::size_t logical_core_count();
SystemInfo system_info();
void validate_config(const BenchmarkConfig& config);
Workload generate_workload(const BenchmarkConfig& config);
ExecutionResult run_serial(const Workload& workload);
ExecutionResult run_parallel(const Workload& workload, std::size_t worker_count);
double median(std::vector<double> values);
AgreementResult compare_results(const Workload& workload,
                                const ExecutionResult& serial,
                                const ExecutionResult& parallel,
                                double tolerance_C = 1e-8);
BenchmarkResult benchmark(const BenchmarkConfig& config);

} // namespace nexus::performance
