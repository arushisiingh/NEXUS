#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nexus::geometry {

struct Vector3 { double x = 0.0; double y = 0.0; double z = 0.0; };
struct BoundingBox { Vector3 minimum; Vector3 maximum; };
struct Triangle { Vector3 normal; Vector3 vertices[3]; };

struct Mesh {
    std::vector<Triangle> triangles;
    BoundingBox bounds;
};

struct GeometryStatistics {
    std::size_t triangle_count = 0;
    BoundingBox bounds;
    double length_x_m = 0.0;
    double length_y_m = 0.0;
    double length_z_m = 0.0;
};

struct TemperatureNode { std::size_t node_id = 0; Vector3 position; double temperature_C = 0.0; };

struct TemperatureDataset {
    std::vector<TemperatureNode> nodes;
    BoundingBox bounds;
    double minimum_temperature_C = 0.0;
    double maximum_temperature_C = 0.0;
    double average_temperature_C = 0.0;
};

Mesh parse_ascii_stl(const std::string& content);
GeometryStatistics geometry_statistics(const Mesh& mesh);
TemperatureDataset parse_temperature_csv(const std::string& content);

} // namespace nexus::geometry
