#include "core/geometry/geometry_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nexus::geometry {
namespace {
bool finite(double value) { return std::isfinite(value); }
std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}
void include_point(BoundingBox& box, const Vector3& point, bool& initialized) {
    if (!initialized) { box.minimum = box.maximum = point; initialized = true; return; }
    box.minimum.x = std::min(box.minimum.x, point.x); box.minimum.y = std::min(box.minimum.y, point.y); box.minimum.z = std::min(box.minimum.z, point.z);
    box.maximum.x = std::max(box.maximum.x, point.x); box.maximum.y = std::max(box.maximum.y, point.y); box.maximum.z = std::max(box.maximum.z, point.z);
}
double parse_number(const std::string& token, const std::string& context) {
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end == token.c_str() || *end != '\0' || !finite(value)) throw std::invalid_argument(context + " must be a finite number");
    return value;
}
std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields; std::stringstream stream(line); std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(trim(field));
    if (!line.empty() && line.back() == ',') fields.emplace_back();
    return fields;
}
}

Mesh parse_ascii_stl(const std::string& content) {
    if (content.empty() || trim(content).empty()) throw std::invalid_argument("STL file is empty");
    const std::string first = trim(content.substr(0, content.find('\n')));
    if (first.rfind("solid", 0) != 0 || content.find('\0') != std::string::npos) throw std::invalid_argument("Binary or unsupported STL format; only ASCII STL is supported");

    Mesh mesh; bool box_initialized = false; bool in_facet = false; bool in_loop = false; bool loop_closed = false; Triangle triangle{}; std::size_t vertex_count = 0; std::size_t line_number = 0; bool saw_solid = false; bool saw_end_solid = false;
    std::istringstream lines(content); std::string line;
    while (std::getline(lines, line)) {
        ++line_number; const std::string normalized = trim(line); if (normalized.empty()) continue;
        std::istringstream tokens(normalized); std::string keyword; tokens >> keyword;
        if (!saw_solid) { if (keyword != "solid") throw std::invalid_argument("Line " + std::to_string(line_number) + ": expected solid"); saw_solid = true; continue; }
        if (keyword == "facet") {
            if (in_facet || in_loop) throw std::invalid_argument("Line " + std::to_string(line_number) + ": nested facet");
            std::string normal_word, x, y, z; if (!(tokens >> normal_word >> x >> y >> z) || normal_word != "normal") throw std::invalid_argument("Line " + std::to_string(line_number) + ": malformed facet normal");
            triangle = {}; triangle.normal = {parse_number(x, "normal x"), parse_number(y, "normal y"), parse_number(z, "normal z")}; in_facet = true; vertex_count = 0; loop_closed = false;
        } else if (keyword == "outer") {
            std::string loop_word; if (!in_facet || in_loop || !(tokens >> loop_word) || loop_word != "loop") throw std::invalid_argument("Line " + std::to_string(line_number) + ": expected outer loop"); in_loop = true;
        } else if (keyword == "vertex") {
            std::string x, y, z; if (!in_loop || vertex_count >= 3 || !(tokens >> x >> y >> z)) throw std::invalid_argument("Line " + std::to_string(line_number) + ": malformed vertex");
            triangle.vertices[vertex_count++] = {parse_number(x, "vertex x"), parse_number(y, "vertex y"), parse_number(z, "vertex z")};
        } else if (keyword == "endloop") {
            if (!in_loop || vertex_count != 3) throw std::invalid_argument("Line " + std::to_string(line_number) + ": facet must contain exactly three vertices"); in_loop = false; loop_closed = true;
        } else if (keyword == "endfacet") {
            if (!in_facet || in_loop || !loop_closed || vertex_count != 3) throw std::invalid_argument("Line " + std::to_string(line_number) + ": incomplete facet");
            mesh.triangles.push_back(triangle); for (const auto& vertex : triangle.vertices) include_point(mesh.bounds, vertex, box_initialized); in_facet = false;
        } else if (keyword == "endsolid") {
            if (in_facet || in_loop) throw std::invalid_argument("Line " + std::to_string(line_number) + ": endsolid before completing facet"); saw_end_solid = true;
        } else if (saw_end_solid) {
            throw std::invalid_argument("Line " + std::to_string(line_number) + ": content after endsolid");
        } else {
            throw std::invalid_argument("Line " + std::to_string(line_number) + ": unexpected STL token '" + keyword + "'");
        }
    }
    if (!saw_solid || !saw_end_solid || mesh.triangles.empty()) throw std::invalid_argument("STL contains no complete triangles");
    return mesh;
}

GeometryStatistics geometry_statistics(const Mesh& mesh) {
    if (mesh.triangles.empty()) throw std::invalid_argument("Cannot calculate statistics for an empty mesh");
    GeometryStatistics stats; stats.triangle_count = mesh.triangles.size(); stats.bounds = mesh.bounds;
    stats.length_x_m = stats.bounds.maximum.x - stats.bounds.minimum.x; stats.length_y_m = stats.bounds.maximum.y - stats.bounds.minimum.y; stats.length_z_m = stats.bounds.maximum.z - stats.bounds.minimum.z;
    return stats;
}

TemperatureDataset parse_temperature_csv(const std::string& content) {
    if (content.empty() || trim(content).empty()) throw std::invalid_argument("Temperature CSV is empty");
    std::istringstream lines(content); std::string line; std::size_t line_number = 0;
    if (!std::getline(lines, line)) throw std::invalid_argument("Temperature CSV is empty");
    ++line_number; const auto header = split_csv(trim(line)); const std::vector<std::string> expected = {"node_id", "x_m", "y_m", "z_m", "temperature_C"};
    if (header != expected) throw std::invalid_argument("Line 1: expected header node_id,x_m,y_m,z_m,temperature_C");
    TemperatureDataset dataset; std::set<std::size_t> ids; bool initialized = false; double sum = 0.0;
    while (std::getline(lines, line)) {
        ++line_number; if (trim(line).empty()) continue; const auto fields = split_csv(line); if (fields.size() != 5) throw std::invalid_argument("Line " + std::to_string(line_number) + ": expected 5 CSV columns");
        char* id_end = nullptr; const unsigned long long raw_id = std::strtoull(fields[0].c_str(), &id_end, 10); if (id_end == fields[0].c_str() || *id_end != '\0') throw std::invalid_argument("Line " + std::to_string(line_number) + ": node_id must be an integer");
        const std::size_t id = static_cast<std::size_t>(raw_id); if (!ids.insert(id).second) throw std::invalid_argument("Line " + std::to_string(line_number) + ": duplicate node_id " + std::to_string(id));
        TemperatureNode node; node.node_id = id; node.position = {parse_number(fields[1], "Line " + std::to_string(line_number) + " x_m"), parse_number(fields[2], "Line " + std::to_string(line_number) + " y_m"), parse_number(fields[3], "Line " + std::to_string(line_number) + " z_m")}; node.temperature_C = parse_number(fields[4], "Line " + std::to_string(line_number) + " temperature_C");
        include_point(dataset.bounds, node.position, initialized); dataset.nodes.push_back(node); dataset.minimum_temperature_C = dataset.nodes.size() == 1 ? node.temperature_C : std::min(dataset.minimum_temperature_C, node.temperature_C); dataset.maximum_temperature_C = dataset.nodes.size() == 1 ? node.temperature_C : std::max(dataset.maximum_temperature_C, node.temperature_C); sum += node.temperature_C;
    }
    if (dataset.nodes.empty()) throw std::invalid_argument("Temperature CSV contains no data rows"); dataset.average_temperature_C = sum / static_cast<double>(dataset.nodes.size()); return dataset;
}
} // namespace nexus::geometry
