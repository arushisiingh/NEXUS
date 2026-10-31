#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>

#include "core/geometry/geometry_parser.hpp"
#include "core/thermal/thermal_solver.hpp"
#include "core/performance/performance_benchmark.hpp"
#include "core/neural/neural_regression.hpp"
#include "core/hardware/hardware_adapters.hpp"

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
using socket_length_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_length_t = socklen_t;
#endif

namespace {
std::atomic<bool> running{true};
nexus::hardware::HardwareManager hardware_manager;

int configured_port(int argc, char** argv) {
    const char* environment_port = std::getenv("NEXUS_API_PORT");
    int port = environment_port ? std::atoi(environment_port) : 8081;
    if (argc > 1) port = std::atoi(argv[1]);
    return port;
}

void close_socket(int socket_fd) {
#ifdef _WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
}

void stop_server(int) { running = false; }

std::string response(const std::string& body, const std::string& status = "200 OK", const std::string& content_type = "application/json") {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n" << body;
    return out.str();
}

constexpr std::size_t MAX_UPLOAD_BYTES = 2U * 1024U * 1024U;

std::string receive_request(int client_fd) {
    std::string request; char buffer[8192]{}; std::size_t content_length = 0; bool has_content_length = false;
    while (true) {
        const int received = static_cast<int>(recv(client_fd, buffer, sizeof(buffer), 0));
        if (received <= 0) break;
        request.append(buffer, static_cast<std::size_t>(received));
        if (request.size() > MAX_UPLOAD_BYTES + 8192U) throw std::invalid_argument("Request exceeds the 2 MiB upload limit");
        const std::size_t header_end = request.find("\r\n\r\n");
        if (header_end == std::string::npos) continue;
        const std::string headers = request.substr(0, header_end);
        const std::string length_marker = "Content-Length:";
        const std::size_t length_position = headers.find(length_marker);
        if (length_position != std::string::npos) {
            const char* start = headers.c_str() + length_position + length_marker.size(); char* end = nullptr;
            content_length = std::strtoull(start, &end, 10); if (end == start || content_length > MAX_UPLOAD_BYTES) throw std::invalid_argument("Request body exceeds the 2 MiB upload limit"); has_content_length = true;
        }
        if (!has_content_length || request.size() >= header_end + 4U + content_length) break;
    }
    return request;
}

std::string static_root() {
    const char* configured = std::getenv("NEXUS_STATIC_ROOT");
    return configured && *configured ? configured : "frontend/dist";
}

std::string mime_type(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") return "application/json";
    return "application/octet-stream";
}

std::optional<std::pair<std::string, std::string>> static_file(const std::string& request) {
    if (request.rfind("GET ", 0) != 0) return std::nullopt;
    const auto path_start = request.find(' ');
    const auto path_end = request.find(' ', path_start + 1);
    if (path_start == std::string::npos || path_end == std::string::npos) return std::nullopt;
    std::string url_path = request.substr(path_start + 1, path_end - path_start - 1);
    if (url_path == "/") url_path = "/index.html";
    if (url_path.find("..") != std::string::npos || url_path.find('?') != std::string::npos) return std::nullopt;
    if (url_path != "/index.html" && url_path.rfind("/assets/", 0) != 0) return std::nullopt;
    const std::string path = static_root() + url_path;
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return std::make_pair(std::move(contents), mime_type(path));
}

std::optional<double> json_number(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t key_position = body.find(marker);
    if (key_position == std::string::npos) return std::nullopt;
    const std::size_t colon = body.find(':', key_position + marker.size());
    if (colon == std::string::npos) throw std::invalid_argument("Missing value for " + key);
    const char* start = body.c_str() + colon + 1;
    char* end = nullptr;
    const double value = std::strtod(start, &end);
    if (end == start) throw std::invalid_argument("Value for " + key + " must be numeric");
    return value;
}

std::string json_string(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\""; const std::size_t key_position = body.find(marker); if (key_position == std::string::npos) throw std::invalid_argument("Missing string field " + key);
    const std::size_t colon = body.find(':', key_position + marker.size()); const std::size_t quote = body.find('"', colon == std::string::npos ? key_position : colon + 1); if (colon == std::string::npos || quote == std::string::npos) throw std::invalid_argument("Field " + key + " must be a string");
    std::string value; bool escaped = false;
    for (std::size_t i = quote + 1; i < body.size(); ++i) { const char c = body[i]; if (escaped) { if (c == 'n') value.push_back('\n'); else if (c == 'r') value.push_back('\r'); else if (c == 't') value.push_back('\t'); else value.push_back(c); escaped = false; } else if (c == '\\') escaped = true; else if (c == '"') return value; else value.push_back(c); }
    throw std::invalid_argument("Unterminated string field " + key);
}

std::size_t json_element_count(const std::string& body) {
    const auto value = json_number(body, "element_count");
    if (!value.has_value() || !std::isfinite(*value) || *value < 0.0 || std::floor(*value) != *value) {
        throw std::invalid_argument("element_count must be an integer");
    }
    return static_cast<std::size_t>(*value);
}

std::size_t json_size_or_default(const std::string& body, const std::string& key, std::size_t fallback) {
    const auto value = json_number(body, key);
    if (!value.has_value()) return fallback;
    if (!std::isfinite(*value) || *value < 0.0 || std::floor(*value) != *value) throw std::invalid_argument(key + " must be an integer");
    return static_cast<std::size_t>(*value);
}

std::vector<std::size_t> json_worker_counts(const std::string& body) {
    const std::string marker = "\"worker_counts\"";
    const auto key_position = body.find(marker);
    if (key_position == std::string::npos) {
        std::vector<std::size_t> defaults{1};
        const auto available = nexus::performance::logical_core_count();
        if (available >= 2) defaults.push_back(2);
        if (available >= 4) defaults.push_back(4);
        return defaults;
    }
    const auto open = body.find('[', key_position + marker.size());
    const auto close = body.find(']', open == std::string::npos ? key_position : open + 1);
    if (open == std::string::npos || close == std::string::npos) throw std::invalid_argument("worker_counts must be an array");
    std::vector<std::size_t> workers;
    const char* cursor = body.c_str() + open + 1;
    const char* end = body.c_str() + close;
    while (cursor < end) {
        while (cursor < end && (*cursor == ' ' || *cursor == ',' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) ++cursor;
        if (cursor >= end) break;
        char* parsed_end = nullptr;
        const double value = std::strtod(cursor, &parsed_end);
        if (parsed_end == cursor || !std::isfinite(value) || value <= 0.0 || std::floor(value) != value) throw std::invalid_argument("worker_counts must contain positive integers");
        workers.push_back(static_cast<std::size_t>(value));
        cursor = parsed_end;
    }
    return workers;
}

double parameter_or_default(const std::string& body, const std::string& key, double fallback) {
    const auto value = json_number(body, key);
    return value.value_or(fallback);
}

std::string json_array(const std::vector<double>& values) {
    std::ostringstream out;
    out << '[' << std::setprecision(17);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ',';
        out << values[i];
    }
    out << ']';
    return out.str();
}

std::string vector_json(const nexus::geometry::Vector3& value) {
    std::ostringstream out; out << std::setprecision(17) << "[" << value.x << ',' << value.y << ',' << value.z << ']'; return out.str();
}

std::string bounding_box_json(const nexus::geometry::BoundingBox& bounds) {
    return std::string("{\"minimum\":") + vector_json(bounds.minimum) + ",\"maximum\":" + vector_json(bounds.maximum) + '}';
}

std::string stl_json(const nexus::geometry::Mesh& mesh, const std::string& filename, double duration_ms) {
    const auto stats = nexus::geometry::geometry_statistics(mesh); std::ostringstream out; out << std::setprecision(17) << "{\"status\":\"success\",\"filename\":\"" << filename << "\",\"triangle_count\":" << stats.triangle_count << ",\"bounding_box\":" << bounding_box_json(stats.bounds) << ",\"dimensions_m\": [" << stats.length_x_m << ',' << stats.length_y_m << ',' << stats.length_z_m << "] ,\"processing_duration_ms\":" << duration_ms << ",\"triangles\":[";
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) { if (i) out << ','; const auto& triangle = mesh.triangles[i]; out << "{\"normal\":" << vector_json(triangle.normal) << ",\"vertices\":[" << vector_json(triangle.vertices[0]) << ',' << vector_json(triangle.vertices[1]) << ',' << vector_json(triangle.vertices[2]) << "]}"; }
    out << "]}"; return out.str();
}

std::string temperature_json(const nexus::geometry::TemperatureDataset& dataset, double duration_ms) {
    std::ostringstream out; out << std::setprecision(17) << "{\"status\":\"success\",\"node_count\":" << dataset.nodes.size() << ",\"minimum_temperature_C\":" << dataset.minimum_temperature_C << ",\"maximum_temperature_C\":" << dataset.maximum_temperature_C << ",\"average_temperature_C\":" << dataset.average_temperature_C << ",\"bounding_box\":" << bounding_box_json(dataset.bounds) << ",\"processing_duration_ms\":" << duration_ms << ",\"nodes\":[";
    for (std::size_t i = 0; i < dataset.nodes.size(); ++i) { if (i) out << ','; const auto& node = dataset.nodes[i]; out << "{\"node_id\":" << node.node_id << ",\"position_m\":" << vector_json(node.position) << ",\"temperature_C\":" << node.temperature_C << '}'; }
    out << "]}"; return out.str();
}

std::string thermal_json(const nexus::thermal::ThermalResult& result, double duration_ms) {
    std::ostringstream out;
    out << std::setprecision(17)
        << "{\"status\":\"success\",\"input_parameters\":{" 
        << "\"length_m\":" << result.parameters.length_m
        << ",\"area_m2\":" << result.parameters.area_m2
        << ",\"conductivity_W_mK\":" << result.parameters.conductivity_W_mK
        << ",\"heat_generation_W_m3\":" << result.parameters.heat_generation_W_m3
        << ",\"left_temperature_C\":" << result.parameters.left_temperature_C
        << ",\"right_temperature_C\":" << result.parameters.right_temperature_C
        << ",\"element_count\":" << result.element_count << "},"
        << "\"nodal_positions_m\":" << json_array(result.nodal_positions_m) << ','
        << "\"nodal_temperatures_C\":" << json_array(result.nodal_temperatures_C) << ','
        << "\"analytical_temperatures_C\":" << json_array(result.analytical_nodal_temperatures_C) << ','
        << "\"midpoint_positions_m\":" << json_array(result.midpoint_positions_m) << ','
        << "\"fem_midpoint_temperatures_C\":" << json_array(result.fem_midpoint_temperatures_C) << ','
        << "\"analytical_midpoint_temperatures_C\":" << json_array(result.analytical_midpoint_temperatures_C) << ','
        << "\"midpoint_absolute_errors_C\":" << json_array(result.midpoint_absolute_errors_C) << ','
        << "\"maximum_absolute_midpoint_error_C\":" << result.maximum_absolute_midpoint_error_C
        << ",\"execution_duration_ms\":" << duration_ms << '}';
    return out.str();
}

std::string performance_json(const nexus::performance::BenchmarkResult& result, double execution_ms) {
    std::ostringstream out;
    out << std::setprecision(17) << "{\"status\":\"success\",\"execution_status\":\"complete\",\"progress\":1,\"workload\":{"
        << "\"seed\":" << result.config.seed << ",\"case_count\":" << result.config.case_count << ",\"mesh_size\":" << result.config.mesh_size << ",\"workload_generation_ms\":" << result.workload_generation_ms << "},"
        << "\"serial\":{" << "\"median_ms\":" << result.serial_median_ms << ",\"measured_runs_ms\":" << json_array(result.serial_measured_runs_ms) << "},"
        << "\"parallel\":[";
    for (std::size_t i = 0; i < result.parallel_timings.size(); ++i) {
        if (i) out << ',';
        const auto& timing = result.parallel_timings[i];
        out << "{\"worker_count\":" << timing.worker_count << ",\"median_ms\":" << timing.median_ms << ",\"measured_runs_ms\":" << json_array(timing.measured_runs_ms)
            << ",\"speedup\":" << timing.speedup << ",\"efficiency\":" << timing.efficiency << '}';
    }
    out << "],\"cpu\":{"
        << "\"model\":\"" << result.system.cpu_model << "\",\"physical_cores\":" << result.system.physical_core_count
        << ",\"logical_cores\":" << result.system.logical_core_count << ",\"total_ram_bytes\":" << result.system.total_ram_bytes
        << ",\"compiler\":\"" << result.system.compiler << "\",\"optimization_flags\":\"" << result.system.optimization_flags << "\"},"
        << "\"numerical_agreement\":{" << "\"maximum_absolute_temperature_difference_C\":" << result.numerical_agreement.maximum_absolute_temperature_difference_C
        << ",\"agreement\":" << (result.numerical_agreement.agreement ? "true" : "false") << ",\"cases_compared\":" << result.numerical_agreement.cases_compared << ",\"failed_case_ids\":[";
    for (std::size_t i = 0; i < result.numerical_agreement.failed_case_ids.size(); ++i) { if (i) out << ','; out << result.numerical_agreement.failed_case_ids[i]; }
    out << "]},\"launch_to_ready_ms\":" << result.launch_to_ready_ms << ",\"execution_duration_ms\":" << execution_ms << '}';
    return out.str();
}

std::string neural_parameters_json(const nexus::neural::Parameters& parameters) {
    std::ostringstream out; out << std::setprecision(17) << "{\"w\":" << parameters.w << ",\"b\":" << parameters.b << ",\"v\":" << parameters.v << ",\"c\":" << parameters.c << '}'; return out.str();
}

std::string neural_json(const nexus::neural::Result& result, double x, double target, double execution_ms) {
    const auto& forward = result.initial_forward;
    const auto& gradient = result.initial_gradients;
    std::ostringstream out;
    out << std::setprecision(17) << "{\"status\":\"success\",\"input\":{"
        << "\"x\":" << x << ",\"target\":" << target << ",\"learning_rate\":" << result.learning_rate << ",\"additional_training_steps\":" << result.additional_training_steps << "},"
        << "\"forward_pass\":{" << "\"z\":" << forward.z << ",\"h\":" << forward.h << ",\"prediction\":" << forward.prediction << ",\"prediction_error\":" << forward.prediction_error << ",\"initial_loss\":" << forward.loss << "},"
        << "\"initial_parameters\":" << neural_parameters_json(result.initial_parameters) << ",\"gradients\":{"
        << "\"dJ_dw\":" << gradient.dJ_dw << ",\"dJ_db\":" << gradient.dJ_db << ",\"dJ_dv\":" << gradient.dJ_dv << ",\"dJ_dc\":" << gradient.dJ_dc << "},"
        << "\"updated_parameters\":" << neural_parameters_json(result.updated_parameters) << ",\"updated_loss\":" << result.updated_loss
        << ",\"gradient_verification_passed\":" << (result.gradient_verification_passed ? "true" : "false") << ",\"finite_difference\":[";
    for (std::size_t i = 0; i < result.finite_difference.size(); ++i) {
        if (i) out << ',';
        const auto& row = result.finite_difference[i];
        out << "{\"parameter\":\"" << row.parameter << "\",\"analytical_gradient\":" << row.analytical_gradient << ",\"numerical_gradient\":" << row.numerical_gradient << ",\"absolute_difference\":" << row.absolute_difference << ",\"pass\":" << (row.pass ? "true" : "false") << '}';
    }
    out << "],\"training_history\":[";
    for (std::size_t i = 0; i < result.training_history.size(); ++i) { if (i) out << ','; out << "{\"step\":" << result.training_history[i].step << ",\"loss\":" << result.training_history[i].loss << '}'; }
    out << "],\"final_parameters\":" << neural_parameters_json(result.final_parameters) << ",\"final_loss\":" << result.final_loss << ",\"execution_duration_ms\":" << execution_ms << '}';
    return out.str();
}

std::string strings_json(const std::vector<std::string>& values) {
    std::ostringstream out; out << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << '"' << values[i] << '"'; } out << ']'; return out.str();
}

std::string hardware_device_json(const nexus::hardware::DeviceInfo& device) {
    std::ostringstream out; out << std::setprecision(17) << "{\"device_id\":\"" << device.device_id << "\",\"device_type\":\"" << nexus::hardware::to_string(device.type) << "\",\"manufacturer\":\"" << device.manufacturer << "\",\"model\":\"" << device.model << "\",\"simulated\":" << (device.simulated ? "true" : "false") << ",\"status\":\"" << nexus::hardware::to_string(device.state) << "\",\"supply_voltage_V\":" << device.supply_voltage_V << ",\"io_voltage_V\":" << device.io_voltage_V << ",\"interfaces\":" << strings_json(device.interfaces) << ",\"capabilities\":" << strings_json(device.capabilities) << ",\"connector\":\"" << device.connector << "\",\"jtag_compatibility\":\"" << device.jtag_compatibility << "\",\"error\":\"" << device.error << "\"}"; return out.str();
}

std::string hardware_devices_json(const std::vector<nexus::hardware::DeviceInfo>& devices) {
    std::ostringstream out; out << "{\"status\":\"success\",\"simulated\":true,\"devices\":["; for (std::size_t i = 0; i < devices.size(); ++i) { if (i) out << ','; out << hardware_device_json(devices[i]); } out << "]}"; return out.str();
}

std::string hardware_telemetry_json(const std::vector<nexus::hardware::TelemetryMessage>& events) {
    std::ostringstream out; out << std::setprecision(17) << "{\"status\":\"success\",\"simulated\":true,\"events\":["; for (std::size_t i = 0; i < events.size(); ++i) { if (i) out << ','; const auto& event = events[i]; out << "{\"sequence\":" << event.sequence << ",\"device_id\":\"" << event.device_id << "\",\"level\":\"" << event.level << "\",\"message\":\"" << event.message << "\",\"status\":\"" << nexus::hardware::to_string(event.state) << "\",\"voltage_V\":" << event.voltage_V << ",\"temperature_C\":" << event.temperature_C << ",\"simulated\":true}"; } out << "]}"; return out.str();
}
}

int main(int argc, char** argv) {
    const int port = configured_port(argc, argv);
    if (port < 1 || port > 65535) {
        std::cerr << "Port must be between 1 and 65535\n";
        return 2;
    }

#ifdef _WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) return 3;
#endif
    std::signal(SIGINT, stop_server);
    std::signal(SIGTERM, stop_server);

    const int server_fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd < 0) {
        std::cerr << "Could not create server socket\n";
        return 4;
    }
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(server_fd, 16) < 0) {
        std::cerr << "Could not bind/listen on 127.0.0.1:" << port << "\n";
        close_socket(server_fd);
        return 5;
    }

    std::cout << "NEXUS API listening on http://127.0.0.1:" << port << "\n";
    while (running) {
        sockaddr_in client{};
        socket_length_t client_size = sizeof(client);
        const int client_fd = static_cast<int>(accept(
            server_fd, reinterpret_cast<sockaddr*>(&client), &client_size));
        if (client_fd < 0) continue;

        std::string request;
        try {
            request = receive_request(client_fd);
        } catch (const std::invalid_argument& error) {
            const std::string output = response(std::string("{\"status\":\"error\",\"error\":\"invalid_request\",\"message\":\"") + error.what() + "\"}", "413 Payload Too Large");
            send(client_fd, output.c_str(), static_cast<int>(output.size()), 0);
            close_socket(client_fd);
            continue;
        }
        if (const auto file = static_file(request); file.has_value()) {
            const std::string output = response(file->first, "200 OK", file->second);
            send(client_fd, output.c_str(), static_cast<int>(output.size()), 0);
            close_socket(client_fd);
            continue;
        }
        const bool health = request.rfind("GET /api/health ", 0) == 0 ||
                            request.rfind("GET /health ", 0) == 0;
        const bool thermal = request.rfind("POST /api/thermal/solve ", 0) == 0;
        const bool performance = request.rfind("POST /api/performance/benchmark ", 0) == 0;
        const bool neural = request.rfind("POST /api/neural/backpropagation ", 0) == 0;
        const bool hardware_devices = request.rfind("GET /api/hardware/devices ", 0) == 0;
        const bool hardware_discover = request.rfind("POST /api/hardware/discover ", 0) == 0;
        const bool hardware_telemetry = request.rfind("GET /api/hardware/telemetry ", 0) == 0;
        const bool hardware_connect = request.rfind("POST /api/hardware/connect ", 0) == 0;
        const bool hardware_disconnect = request.rfind("POST /api/hardware/disconnect ", 0) == 0;
        const bool hardware_failure = request.rfind("POST /api/hardware/simulate-failure ", 0) == 0;
        const bool hardware_recover = request.rfind("POST /api/hardware/recover ", 0) == 0;
        const bool stl = request.rfind("POST /api/geometry/stl/parse ", 0) == 0;
        const bool temperature_csv = request.rfind("POST /api/geometry/csv/parse ", 0) == 0;
        std::string body;
        std::string status = "200 OK";
        if (health) {
            body = R"({"application_name":"NEXUS","application_version":"0.1.0","backend_status":"online","cpp_standard":"C++17","api_status":"ok"})";
        } else if (thermal) {
            try {
                const std::size_t body_start = request.find("\r\n\r\n");
                if (body_start == std::string::npos) throw std::invalid_argument("Request body is missing");
                const std::string payload = request.substr(body_start + 4);
                nexus::thermal::ThermalParameters parameters;
                parameters.length_m = parameter_or_default(payload, "length_m", parameters.length_m);
                parameters.area_m2 = parameter_or_default(payload, "area_m2", parameters.area_m2);
                parameters.conductivity_W_mK = parameter_or_default(payload, "conductivity_W_mK", parameters.conductivity_W_mK);
                parameters.heat_generation_W_m3 = parameter_or_default(payload, "heat_generation_W_m3", parameters.heat_generation_W_m3);
                parameters.left_temperature_C = parameter_or_default(payload, "left_temperature_C", parameters.left_temperature_C);
                parameters.right_temperature_C = parameter_or_default(payload, "right_temperature_C", parameters.right_temperature_C);
                const std::size_t elements = json_element_count(payload);
                const auto start = std::chrono::steady_clock::now();
                const auto result = nexus::thermal::solve(parameters, elements);
                const auto end = std::chrono::steady_clock::now();
                const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
                body = thermal_json(result, duration_ms);
            } catch (const std::invalid_argument& error) {
                status = "400 Bad Request";
                body = std::string(R"({"status":"error","error":"invalid_request","message":")") + error.what() + "\"}";
            } catch (const std::exception& error) {
                status = "500 Internal Server Error";
                body = std::string(R"({"status":"error","error":"simulation_failed","message":")") + error.what() + "\"}";
            }
        } else if (hardware_devices || hardware_discover) {
            try { body = hardware_devices_json(hardware_discover ? hardware_manager.discover() : hardware_manager.devices()); }
            catch (const std::exception& error) { status = "500 Internal Server Error"; body = std::string(R"({"status":"error","error":"hardware_discovery_failed","message":")") + error.what() + "\"}"; }
        } else if (hardware_telemetry) {
            body = hardware_telemetry_json(hardware_manager.telemetry());
        } else if (hardware_connect || hardware_disconnect || hardware_failure || hardware_recover) {
            try {
                const std::size_t body_start = request.find("\r\n\r\n"); if (body_start == std::string::npos) throw std::invalid_argument("Request body is missing");
                const std::string device_id = json_string(request.substr(body_start + 4), "device_id");
                nexus::hardware::DeviceInfo device;
                if (hardware_connect) device = hardware_manager.connect(device_id);
                else if (hardware_disconnect) device = hardware_manager.disconnect(device_id);
                else if (hardware_failure) device = hardware_manager.simulate_failure(device_id);
                else device = hardware_manager.recover(device_id);
                body = std::string("{\"status\":\"success\",\"simulated\":true,\"device\":") + hardware_device_json(device) + "}";
            } catch (const std::invalid_argument& error) {
                status = "400 Bad Request"; body = std::string(R"({"status":"error","error":"invalid_request","message":")") + error.what() + "\"}";
            } catch (const std::exception& error) {
                status = "409 Conflict"; body = std::string(R"({"status":"error","error":"hardware_operation_failed","message":")") + error.what() + "\"}";
            }
        } else if (performance) {
            try {
                const std::size_t body_start = request.find("\r\n\r\n");
                if (body_start == std::string::npos) throw std::invalid_argument("Request body is missing");
                const std::string payload = request.substr(body_start + 4);
                nexus::performance::BenchmarkConfig config;
                config.case_count = json_size_or_default(payload, "case_count", config.case_count);
                config.mesh_size = json_size_or_default(payload, "mesh_size", config.mesh_size);
                if (const auto seed = json_number(payload, "seed"); seed.has_value()) {
                    if (!std::isfinite(*seed) || *seed < 0.0 || std::floor(*seed) != *seed) throw std::invalid_argument("seed must be a non-negative integer");
                    config.seed = static_cast<std::uint64_t>(*seed);
                }
                config.worker_counts = json_worker_counts(payload);
                nexus::performance::validate_config(config);
                const auto start = std::chrono::steady_clock::now();
                const auto result = nexus::performance::benchmark(config);
                const auto end = std::chrono::steady_clock::now();
                body = performance_json(result, std::chrono::duration<double, std::milli>(end - start).count());
            } catch (const std::invalid_argument& error) {
                status = "400 Bad Request";
                body = std::string(R"({"status":"error","error":"invalid_request","message":")") + error.what() + "\"}";
            } catch (const std::exception& error) {
                status = "500 Internal Server Error";
                body = std::string(R"({"status":"error","error":"benchmark_failed","message":")") + error.what() + "\"}";
            }
        } else if (neural) {
            try {
                const std::size_t body_start = request.find("\r\n\r\n");
                if (body_start == std::string::npos) throw std::invalid_argument("Request body is missing");
                const std::string payload = request.substr(body_start + 4);
                const double x = parameter_or_default(payload, "x", 1.0);
                const double target = parameter_or_default(payload, "target", 2.0);
                nexus::neural::Parameters parameters;
                parameters.w = parameter_or_default(payload, "w", parameters.w);
                parameters.b = parameter_or_default(payload, "b", parameters.b);
                parameters.v = parameter_or_default(payload, "v", parameters.v);
                parameters.c = parameter_or_default(payload, "c", parameters.c);
                const double learning_rate = parameter_or_default(payload, "learning_rate", 0.1);
                const std::size_t training_steps = json_size_or_default(payload, "additional_training_steps", 100);
                const auto start = std::chrono::steady_clock::now();
                const auto result = nexus::neural::solve(x, target, parameters, learning_rate, training_steps);
                const auto end = std::chrono::steady_clock::now();
                body = neural_json(result, x, target, std::chrono::duration<double, std::milli>(end - start).count());
            } catch (const std::invalid_argument& error) {
                status = "400 Bad Request";
                body = std::string(R"({"status":"error","error":"invalid_request","message":")") + error.what() + "\"}";
            } catch (const std::exception& error) {
                status = "500 Internal Server Error";
                body = std::string(R"({"status":"error","error":"neural_failed","message":")") + error.what() + "\"}";
            }
        } else if (stl || temperature_csv) {
            try {
                const std::size_t body_start = request.find("\r\n\r\n");
                if (body_start == std::string::npos) throw std::invalid_argument("Request body is missing");
                const std::string payload = request.substr(body_start + 4);
                const std::string content = json_string(payload, "content");
                const std::string filename = json_string(payload, "filename");
                const auto start = std::chrono::steady_clock::now();
                if (stl) {
                    const auto mesh = nexus::geometry::parse_ascii_stl(content);
                    const auto end = std::chrono::steady_clock::now();
                    body = stl_json(mesh, filename.empty() ? "uploaded.stl" : filename, std::chrono::duration<double, std::milli>(end - start).count());
                } else {
                    const auto dataset = nexus::geometry::parse_temperature_csv(content);
                    const auto end = std::chrono::steady_clock::now();
                    body = temperature_json(dataset, std::chrono::duration<double, std::milli>(end - start).count());
                }
            } catch (const std::invalid_argument& error) {
                status = "400 Bad Request";
                body = std::string("{\"status\":\"error\",\"error\":\"invalid_file\",\"message\":\"") + error.what() + "\"}";
            } catch (const std::exception& error) {
                status = "500 Internal Server Error";
                body = std::string("{\"status\":\"error\",\"error\":\"parse_failed\",\"message\":\"") + error.what() + "\"}";
            }
        } else {
            status = "404 Not Found";
            body = R"({"status":"error","error":"route_not_found","message":"Use the documented NEXUS API routes, including /api/hardware/devices, /api/hardware/discover, /api/hardware/connect, /api/hardware/disconnect, /api/hardware/simulate-failure, /api/hardware/recover, and /api/hardware/telemetry"})";
        }
        const std::string output = response(body, status);
        send(client_fd, output.c_str(), static_cast<int>(output.size()), 0);
        close_socket(client_fd);
    }

    close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
