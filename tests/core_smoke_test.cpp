#include <iostream>
#include <cmath>
#include <limits>
#include <string>

#include "core/thermal/thermal_solver.hpp"
#include "core/geometry/geometry_parser.hpp"
#include "core/performance/performance_benchmark.hpp"
#include "core/neural/neural_regression.hpp"
#include "core/hardware/hardware_adapters.hpp"

namespace {
void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        const std::string product = "NEXUS";
        expect(product == "NEXUS", "Product identity check failed");
        nexus::thermal::ThermalParameters parameters;
        double previous_error = std::numeric_limits<double>::infinity();
        for (const std::size_t elements : {10U, 20U, 40U, 80U}) {
            const auto result = nexus::thermal::solve(parameters, elements);
            expect(result.nodal_temperatures_C.size() == elements + 1, "Unexpected node count");
            expect(std::abs(result.nodal_temperatures_C.front() - 100.0) < 1e-12, "Left boundary mismatch");
            expect(std::abs(result.nodal_temperatures_C.back() - 20.0) < 1e-12, "Right boundary mismatch");
            expect(result.maximum_absolute_midpoint_error_C < previous_error, "Mesh refinement did not reduce midpoint error");
            previous_error = result.maximum_absolute_midpoint_error_C;
        }

        parameters.heat_generation_W_m3 = 0.0;
        const auto zero_heat = nexus::thermal::solve(parameters, 10);
        expect(zero_heat.maximum_absolute_midpoint_error_C < 1e-12, "Zero-heat linear solution mismatch");
        expect(std::abs(zero_heat.nodal_temperatures_C[5] - 60.0) < 1e-12, "Zero-heat midpoint mismatch");

        for (const auto invalid : {
            nexus::thermal::ThermalParameters{1.0, 1e-4, 0.0, 10000.0, 100.0, 20.0},
            nexus::thermal::ThermalParameters{1.0, 0.0, 10.0, 10000.0, 100.0, 20.0},
            nexus::thermal::ThermalParameters{1.0, 1e-4, std::numeric_limits<double>::quiet_NaN(), 10000.0, 100.0, 20.0},
        }) {
            bool rejected = false;
            try { (void)nexus::thermal::solve(invalid, 10); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected, "Invalid physical parameter was accepted");
        }
        bool invalid_mesh_rejected = false;
        try { (void)nexus::thermal::solve(parameters, 0); } catch (const std::invalid_argument&) { invalid_mesh_rejected = true; }
        expect(invalid_mesh_rejected, "Invalid mesh size was accepted");

        const std::string stl =
            "solid cube\n"
            " facet normal 0 0 1\n"
            "  outer loop\n"
            "   vertex 0 0 0\n"
            "   vertex 1 0 0\n"
            "   vertex 0 1 0\n"
            "  endloop\n"
            " endfacet\n"
            "endsolid cube\n";
        const auto mesh = nexus::geometry::parse_ascii_stl(stl);
        expect(mesh.triangles.size() == 1, "STL triangle count mismatch");
        const auto mesh_stats = nexus::geometry::geometry_statistics(mesh);
        expect(std::abs(mesh_stats.length_x_m - 1.0) < 1e-12, "STL bounds mismatch");
        bool malformed_stl_rejected = false;
        try { (void)nexus::geometry::parse_ascii_stl("solid bad\n facet normal 0 0 1\n endfacet\nendsolid bad\n"); } catch (const std::invalid_argument&) { malformed_stl_rejected = true; }
        expect(malformed_stl_rejected, "Malformed STL was accepted");
        for (const std::string& invalid_stl : {
            std::string{},
            std::string("not an STL"),
            std::string("solid nan\n facet normal nan 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n endfacet\nendsolid nan\n"),
            std::string("solid incomplete\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n  endloop\n endfacet\nendsolid incomplete\n")
        }) {
            bool rejected = false;
            try { (void)nexus::geometry::parse_ascii_stl(invalid_stl); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected, "Invalid STL was accepted");
        }

        const std::string csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,100\n2,1,0,0,80\n";
        const auto dataset = nexus::geometry::parse_temperature_csv(csv);
        expect(dataset.nodes.size() == 2, "CSV node count mismatch");
        expect(std::abs(dataset.average_temperature_C - 90.0) < 1e-12, "CSV average mismatch");
        bool duplicate_csv_rejected = false;
        try { (void)nexus::geometry::parse_temperature_csv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,100\n1,1,0,0,80\n"); } catch (const std::invalid_argument&) { duplicate_csv_rejected = true; }
        expect(duplicate_csv_rejected, "Duplicate CSV node was accepted");
        for (const std::string& invalid_csv : {
            std::string{},
            std::string("wrong,header\n1,0,0,0,100\n"),
            std::string("node_id,x_m,y_m,z_m,temperature_C\n1,nan,0,0,100\n"),
            std::string("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,inf\n"),
            std::string("node_id,x_m,y_m,z_m,temperature_C\n1,0,0\n")
        }) {
            bool rejected = false;
            try { (void)nexus::geometry::parse_temperature_csv(invalid_csv); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected, "Invalid temperature CSV was accepted");
        }
        nexus::performance::BenchmarkConfig benchmark_config;
        benchmark_config.case_count = 6;
        benchmark_config.mesh_size = 10;
        benchmark_config.seed = 42;
        benchmark_config.worker_counts = {1, 2};
        const auto workload_a = nexus::performance::generate_workload(benchmark_config);
        const auto workload_b = nexus::performance::generate_workload(benchmark_config);
        expect(workload_a.cases.size() == benchmark_config.case_count, "Workload case count mismatch");
        for (std::size_t index = 0; index < workload_a.cases.size(); ++index) {
            expect(workload_a.cases[index].parameters.conductivity_W_mK > 0.0, "Generated conductivity is not positive");
            expect(workload_a.cases[index].parameters.heat_generation_W_m3 > 0.0, "Generated heat generation is not positive");
            expect(workload_a.cases[index].parameters.conductivity_W_mK == workload_b.cases[index].parameters.conductivity_W_mK, "Workload is not reproducible");
            expect(workload_a.cases[index].parameters.heat_generation_W_m3 == workload_b.cases[index].parameters.heat_generation_W_m3, "Workload heat generation is not reproducible");
        }
        const auto serial = nexus::performance::run_serial(workload_a);
        const auto parallel_one = nexus::performance::run_parallel(workload_a, 1);
        const auto parallel_two = nexus::performance::run_parallel(workload_a, 2);
        expect(serial.case_results.size() == benchmark_config.case_count, "Serial result count mismatch");
        expect(parallel_two.case_results.size() == benchmark_config.case_count, "Parallel result count mismatch");
        const auto agreement = nexus::performance::compare_results(workload_a, serial, parallel_two);
        expect(agreement.agreement, "Serial and parallel results disagree");
        expect(agreement.maximum_absolute_temperature_difference_C <= 1e-8, "Numerical agreement tolerance exceeded");
        for (std::size_t index = 0; index < serial.case_results.size(); ++index) {
            expect(std::abs(serial.case_results[index].nodal_temperatures_C.front() - 100.0) < 1e-12, "Benchmark left boundary mismatch");
            expect(std::abs(serial.case_results[index].nodal_temperatures_C.back() - 20.0) < 1e-12, "Benchmark right boundary mismatch");
            expect(std::abs(serial.case_results[index].nodal_temperatures_C.front() - parallel_one.case_results[index].nodal_temperatures_C.front()) < 1e-12, "One-worker ordering mismatch");
        }
        expect(nexus::performance::median({5.0, 1.0, 3.0, 7.0}) == 4.0, "Median calculation mismatch");
        for (const auto invalid : {
            nexus::performance::BenchmarkConfig{0, 10, 1, {1}},
            nexus::performance::BenchmarkConfig{1, 0, 1, {1}},
            nexus::performance::BenchmarkConfig{1, 10, 1, {0}}
        }) {
            bool rejected = false;
            try { nexus::performance::validate_config(invalid); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected, "Invalid benchmark configuration was accepted");
        }
        const auto neural = nexus::neural::solve();
        expect(std::abs(neural.initial_forward.z - 0.5) < 1e-12, "Neural z mismatch");
        expect(std::abs(neural.initial_forward.h - std::tanh(0.5)) < 1e-12, "Neural hidden activation mismatch");
        expect(std::abs(neural.initial_forward.prediction - std::tanh(0.5)) < 1e-12, "Neural prediction mismatch");
        expect(neural.initial_forward.loss > 1.18 && neural.initial_forward.loss < 1.19, "Neural initial loss mismatch");
        expect(neural.gradient_verification_passed, "Neural finite-difference verification failed");
        for (const auto& row : neural.finite_difference) expect(row.absolute_difference <= 1e-5, "Neural finite-difference tolerance exceeded");
        expect(std::abs(neural.updated_parameters.w - (0.5 - 0.1 * neural.initial_gradients.dJ_dw)) < 1e-12, "Neural simultaneous w update mismatch");
        expect(std::abs(neural.updated_parameters.b - (0.0 - 0.1 * neural.initial_gradients.dJ_db)) < 1e-12, "Neural simultaneous b update mismatch");
        expect(neural.training_history.size() == 101, "Neural training history length mismatch");
        expect(neural.final_loss < neural.updated_loss, "Neural training did not reduce loss");
        for (const auto invalid : {
            nexus::neural::Parameters{std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0, 0.0},
            nexus::neural::Parameters{0.5, 0.0, std::numeric_limits<double>::infinity(), 0.0}
        }) {
            bool rejected = false;
            try { (void)nexus::neural::solve(1.0, 2.0, invalid, 0.1, 100); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected, "Non-finite neural parameters were accepted");
        }
        bool invalid_neural_input_rejected = false;
        try { (void)nexus::neural::solve(1.0, 2.0, nexus::neural::Parameters{}, 0.0, 100); } catch (const std::invalid_argument&) { invalid_neural_input_rejected = true; }
        expect(invalid_neural_input_rejected, "Invalid neural learning rate was accepted");
        nexus::hardware::HardwareManager hardware;
        const auto discovered = hardware.discover();
        expect(discovered.size() == 3, "Hardware discovery count mismatch");
        expect(discovered[0].type == nexus::hardware::DeviceType::Microcontroller, "Microcontroller discovery mismatch");
        expect(discovered[1].type == nexus::hardware::DeviceType::Fpga, "FPGA discovery mismatch");
        expect(discovered[2].type == nexus::hardware::DeviceType::JtagAdapter, "JTAG discovery mismatch");
        expect(discovered[0].simulated && discovered[1].simulated && discovered[2].simulated, "Hardware simulation label missing");
        for (const auto& device : discovered) {
            expect(!device.capabilities.empty(), "Hardware capability report is empty");
            const auto connected = hardware.connect(device.device_id);
            expect(connected.state == nexus::hardware::ConnectionState::Connected, "Hardware connection failed");
            const auto disconnected = hardware.disconnect(device.device_id);
            expect(disconnected.state == nexus::hardware::ConnectionState::Disconnected, "Hardware disconnection failed");
            (void)hardware.connect(device.device_id);
            const auto fault = hardware.simulate_failure(device.device_id);
            expect(fault.state == nexus::hardware::ConnectionState::Fault, "Hardware timeout simulation failed");
            const auto recovered = hardware.recover(device.device_id);
            expect(recovered.state == nexus::hardware::ConnectionState::Connected, "Hardware recovery failed");
        }
        expect(!hardware.telemetry().empty(), "Hardware telemetry is empty");
        bool invalid_device_rejected = false;
        try { (void)hardware.connect("missing-device"); } catch (const std::invalid_argument&) { invalid_device_rejected = true; }
        expect(invalid_device_rejected, "Invalid hardware device ID was accepted");
        bool repeated_failure_rejected = false;
        (void)hardware.simulate_failure("arduino-nano-rp2040-connect");
        try { (void)hardware.simulate_failure("arduino-nano-rp2040-connect"); } catch (const std::runtime_error&) { repeated_failure_rejected = true; }
        expect(repeated_failure_rejected, "Repeated hardware failure was accepted");
        std::cout << "thermal solver tests: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "thermal solver tests: FAIL — " << error.what() << '\n';
        return 1;
    }
}
