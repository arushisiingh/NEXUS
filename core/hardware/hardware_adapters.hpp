#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::hardware {

enum class DeviceType { Microcontroller, Fpga, JtagAdapter };
enum class ConnectionState { Discovered, Connected, Disconnected, Fault };

std::string to_string(DeviceType type);
std::string to_string(ConnectionState state);

struct DeviceInfo {
    std::string device_id;
    DeviceType type = DeviceType::Microcontroller;
    std::string manufacturer;
    std::string model;
    double supply_voltage_V = 0.0;
    double io_voltage_V = 0.0;
    std::vector<std::string> interfaces;
    std::vector<std::string> capabilities;
    std::string connector;
    std::string jtag_compatibility;
    ConnectionState state = ConnectionState::Disconnected;
    bool simulated = true;
    std::string error;
};

struct TelemetryMessage {
    std::size_t sequence = 0;
    std::string device_id;
    std::string level;
    std::string message;
    ConnectionState state = ConnectionState::Disconnected;
    double voltage_V = 0.0;
    double temperature_C = 0.0;
    bool simulated = true;
};

class IHardwareAdapter {
public:
    virtual ~IHardwareAdapter() = default;
    virtual const DeviceInfo& discover() = 0;
    virtual const DeviceInfo& capabilities() const = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void simulate_timeout() = 0;
    virtual void recover() = 0;
    virtual TelemetryMessage telemetry(std::size_t sequence) const = 0;
};

class HardwareManager {
public:
    HardwareManager();
    std::vector<DeviceInfo> discover();
    DeviceInfo connect(const std::string& device_id);
    DeviceInfo disconnect(const std::string& device_id);
    DeviceInfo simulate_failure(const std::string& device_id);
    DeviceInfo recover(const std::string& device_id);
    std::vector<DeviceInfo> devices() const;
    std::vector<TelemetryMessage> telemetry() const;

private:
    IHardwareAdapter& adapter(const std::string& device_id);
    const IHardwareAdapter& adapter(const std::string& device_id) const;
    void record(const std::string& device_id, const std::string& level, const std::string& message);
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<IHardwareAdapter>> adapters_;
    std::vector<TelemetryMessage> events_;
    std::size_t sequence_ = 0;
};

} // namespace nexus::hardware
