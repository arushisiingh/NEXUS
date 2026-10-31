#include "core/performance/performance_benchmark.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace nexus::performance {
namespace {

bool finite(double value) { return std::isfinite(value); }

std::string read_cpu_model() {
#ifdef __APPLE__
    char buffer[256]{};
    std::size_t size = sizeof(buffer);
    if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) return buffer;
#elif defined(__linux__)
    std::ifstream input("/proc/cpuinfo");
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("model name", 0) == 0) {
            const auto separator = line.find(':');
            if (separator != std::string::npos) return line.substr(separator + 2);
        }
    }
#endif
    return "Unknown CPU model";
}

std::size_t physical_core_count_impl() {
#ifdef __APPLE__
    int value = 1;
    std::size_t size = sizeof(value);
    if (sysctlbyname("hw.physicalcpu", &value, &size, nullptr, 0) == 0 && value > 0) return static_cast<std::size_t>(value);
#elif defined(__linux__)
    std::ifstream input("/sys/devices/system/cpu/cpu0/topology/core_cpus_list");
    std::string value;
    if (std::getline(input, value) && !value.empty()) return std::max<std::size_t>(1, std::count(value.begin(), value.end(), ',') + 1);
#endif
    return logical_core_count();
}

std::uint64_t total_ram_bytes_impl() {
#ifdef __APPLE__
    std::uint64_t value = 0;
    std::size_t size = sizeof(value);
    if (sysctlbyname("hw.memsize", &value, &size, nullptr, 0) == 0) return value;
#elif defined(__linux__)
    struct sysinfo info{};
    if (sysinfo(&info) == 0) return static_cast<std::uint64_t>(info.totalram) * info.mem_unit;
#endif
    return 0;
}

std::string compiler_name() {
#if defined(__clang__)
    return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#elif defined(_MSC_VER)
    return std::string("MSVC ") + std::to_string(_MSC_VER);
#else
    return "Unknown compiler";
#endif
}

std::vector<double> measured_runs(const Workload& workload, std::size_t worker_count) {
    (void)(worker_count == 1 ? run_serial(workload) : run_parallel(workload, worker_count));
    std::vector<double> timings;
    timings.reserve(5);
    for (int repetition = 0; repetition < 5; ++repetition) {
        const auto result = worker_count == 1 ? run_serial(workload) : run_parallel(workload, worker_count);
        timings.push_back(result.elapsed_ms);
    }
    return timings;
}

} // namespace

std::size_t logical_core_count() {
    return std::max<std::size_t>(1, std::thread::hardware_concurrency());
}

SystemInfo system_info() {
    SystemInfo info;
    info.cpu_model = read_cpu_model();
    info.logical_core_count = logical_core_count();
    info.physical_core_count = std::min(physical_core_count_impl(), info.logical_core_count);
    info.total_ram_bytes = total_ram_bytes_impl();
    info.compiler = compiler_name();
#ifdef __OPTIMIZE__
    info.optimization_flags = "compiler optimization enabled";
#else
    info.optimization_flags = "no optimization macro detected";
#endif
    return info;
}

void validate_config(const BenchmarkConfig& config) {
    if (config.case_count == 0 || config.case_count > 10000) throw std::invalid_argument("case_count must be between 1 and 10000");
    if (config.mesh_size == 0 || config.mesh_size > 10000) throw std::invalid_argument("mesh_size must be between 1 and 10000");
    if (config.worker_counts.empty()) throw std::invalid_argument("worker_counts must not be empty");
    const auto available = logical_core_count();
    for (const auto workers : config.worker_counts) {
        if (workers == 0 || workers > 4 || workers > available) throw std::invalid_argument("worker counts must be between 1 and min(4, logical CPUs)");
    }
}

Workload generate_workload(const BenchmarkConfig& config) {
    validate_config(config);
    std::mt19937_64 generator(config.seed);
    std::uniform_real_distribution<double> conductivity(5.0, 50.0);
    std::uniform_real_distribution<double> heat_generation(1000.0, 20000.0);
    Workload workload{config.seed, config.mesh_size, {}};
    workload.cases.reserve(config.case_count);
    for (std::size_t index = 0; index < config.case_count; ++index) {
        thermal::ThermalParameters parameters;
        parameters.conductivity_W_mK = conductivity(generator);
        parameters.heat_generation_W_m3 = heat_generation(generator);
        workload.cases.push_back({index, parameters});
    }
    return workload;
}

ExecutionResult run_serial(const Workload& workload) {
    const auto start = std::chrono::steady_clock::now();
    ExecutionResult result;
    result.case_results.reserve(workload.cases.size());
    for (const auto& item : workload.cases) result.case_results.push_back(thermal::solve(item.parameters, workload.mesh_size));
    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

ExecutionResult run_parallel(const Workload& workload, std::size_t worker_count) {
    if (worker_count == 0 || worker_count > logical_core_count()) throw std::invalid_argument("worker_count exceeds available logical CPUs");
    const auto start = std::chrono::steady_clock::now();
    ExecutionResult result;
    result.case_results.resize(workload.cases.size());
    std::atomic<std::size_t> next_case{0};
    std::mutex exception_mutex;
    std::exception_ptr first_exception;
    const std::size_t actual_workers = std::min(worker_count, std::max<std::size_t>(1, workload.cases.size()));
    std::vector<std::thread> workers;
    workers.reserve(actual_workers);
    for (std::size_t worker = 0; worker < actual_workers; ++worker) {
        workers.emplace_back([&]() {
            try {
                while (true) {
                    const auto index = next_case.fetch_add(1, std::memory_order_relaxed);
                    if (index >= workload.cases.size()) break;
                    result.case_results[index] = thermal::solve(workload.cases[index].parameters, workload.mesh_size);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(exception_mutex);
                if (!first_exception) first_exception = std::current_exception();
            }
        });
    }
    for (auto& worker : workers) worker.join();
    if (first_exception) std::rethrow_exception(first_exception);
    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

double median(std::vector<double> values) {
    if (values.empty()) throw std::invalid_argument("Cannot calculate median of empty data");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) / 2.0 : values[middle];
}

AgreementResult compare_results(const Workload& workload, const ExecutionResult& serial, const ExecutionResult& parallel, double tolerance_C) {
    if (serial.case_results.size() != workload.cases.size() || parallel.case_results.size() != workload.cases.size()) throw std::invalid_argument("Execution result count does not match workload");
    AgreementResult agreement;
    agreement.cases_compared = workload.cases.size();
    for (std::size_t index = 0; index < workload.cases.size(); ++index) {
        const auto& left = serial.case_results[index].nodal_temperatures_C;
        const auto& right = parallel.case_results[index].nodal_temperatures_C;
        if (left.size() != right.size()) { agreement.failed_case_ids.push_back(workload.cases[index].case_id); continue; }
        double maximum = 0.0;
        for (std::size_t node = 0; node < left.size(); ++node) maximum = std::max(maximum, std::abs(left[node] - right[node]));
        agreement.maximum_absolute_temperature_difference_C = std::max(agreement.maximum_absolute_temperature_difference_C, maximum);
        if (!finite(maximum) || maximum > tolerance_C) agreement.failed_case_ids.push_back(workload.cases[index].case_id);
    }
    agreement.agreement = agreement.failed_case_ids.empty();
    return agreement;
}

BenchmarkResult benchmark(const BenchmarkConfig& config) {
    const auto launch_start = std::chrono::steady_clock::now();
    const auto workload_start = std::chrono::steady_clock::now();
    const Workload workload = generate_workload(config);
    const auto workload_end = std::chrono::steady_clock::now();
    BenchmarkResult result;
    result.config = config;
    result.workload = workload;
    result.system = system_info();
    result.workload_generation_ms = std::chrono::duration<double, std::milli>(workload_end - workload_start).count();
    result.launch_to_ready_ms = std::chrono::duration<double, std::milli>(workload_end - launch_start).count();
    result.serial_measured_runs_ms = measured_runs(workload, 1);
    result.serial_median_ms = median(result.serial_measured_runs_ms);
    const auto serial_final = run_serial(workload);
    for (const auto worker_count : config.worker_counts) {
        WorkerTiming timing;
        timing.worker_count = worker_count;
        timing.measured_runs_ms = measured_runs(workload, worker_count);
        timing.median_ms = median(timing.measured_runs_ms);
        timing.speedup = result.serial_median_ms / timing.median_ms;
        timing.efficiency = timing.speedup / static_cast<double>(worker_count);
        const auto parallel_final = run_parallel(workload, worker_count);
        const auto agreement = compare_results(workload, serial_final, parallel_final);
        if (worker_count == config.worker_counts.front()) result.numerical_agreement = agreement;
        else {
            result.numerical_agreement.maximum_absolute_temperature_difference_C = std::max(result.numerical_agreement.maximum_absolute_temperature_difference_C, agreement.maximum_absolute_temperature_difference_C);
            result.numerical_agreement.failed_case_ids.insert(result.numerical_agreement.failed_case_ids.end(), agreement.failed_case_ids.begin(), agreement.failed_case_ids.end());
            result.numerical_agreement.agreement = result.numerical_agreement.agreement && agreement.agreement;
        }
        result.parallel_timings.push_back(std::move(timing));
    }
    return result;
}

} // namespace nexus::performance
