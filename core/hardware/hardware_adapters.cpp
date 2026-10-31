#include "core/hardware/hardware_adapters.hpp"

#include <stdexcept>
#include <utility>

namespace nexus::hardware {
namespace {

class MockHardwareAdapter : public IHardwareAdapter {
public:
    explicit MockHardwareAdapter(DeviceInfo info) : info_(std::move(info)) {}
    const DeviceInfo& discover() override { info_.state = ConnectionState::Discovered; info_.error.clear(); return info_; }
    const DeviceInfo& capabilities() const override { return info_; }
    void connect() override {
        if (info_.state == ConnectionState::Fault) throw std::runtime_error("simulated connection timeout; recover the device before reconnecting");
        info_.state = ConnectionState::Connected; info_.error.clear();
    }
    void disconnect() override { info_.state = ConnectionState::Disconnected; info_.error.clear(); }
    void simulate_timeout() override {
        if (info_.state != ConnectionState::Connected) throw std::runtime_error("device must be connected before simulating a timeout");
        info_.state = ConnectionState::Fault; info_.error = "Simulated connection timeout";
    }
    void recover() override {
        if (info_.state != ConnectionState::Fault) throw std::runtime_error("device is not in a recoverable fault state");
        info_.state = ConnectionState::Connected; info_.error.clear();
    }
    TelemetryMessage telemetry(std::size_t sequence) const override {
        const double nominal_voltage = info_.io_voltage_V;
        const double nominal_temperature = info_.state == ConnectionState::Connected ? 29.5 : 24.0;
        return {sequence, info_.device_id, info_.state == ConnectionState::Fault ? "error" : "info", info_.state == ConnectionState::Fault ? info_.error : "Simulated telemetry heartbeat", info_.state, nominal_voltage, nominal_temperature, true};
    }
private:
    DeviceInfo info_;
};

std::unique_ptr<IHardwareAdapter> microcontroller() {
    return std::make_unique<MockHardwareAdapter>(DeviceInfo{"arduino-nano-rp2040-connect", DeviceType::Microcontroller, "Arduino", "Nano RP2040 Connect", 5.0, 3.3, {"USB 1.1 device", "Wi-Fi", "Bluetooth LE", "I2C", "SPI", "UART"}, {"RP2040 dual-core Cortex-M0+", "133 MHz", "264 KB SRAM", "16 MB flash", "SWD debug pads"}, "Nano 2.54 mm headers / USB Micro-B", "SWD for RP2040; not a JTAG target", ConnectionState::Disconnected, true, ""});
}

std::unique_ptr<IHardwareAdapter> fpga() {
    return std::make_unique<MockHardwareAdapter>(DeviceInfo{"digilent-basys3", DeviceType::Fpga, "Digilent", "Basys 3 (Artix-7 35T)", 5.0, 3.3, {"USB-JTAG/UART", "JTAG", "UART", "Pmod", "VGA"}, {"Artix-7 FPGA", "USB-JTAG programming", "16 LEDs", "16 switches", "XADC"}, "Micro-AB USB J4 / 6-pin JTAG J5 / Pmod", "JTAG programming through the on-board USB-JTAG circuit", ConnectionState::Disconnected, true, ""});
}

std::unique_ptr<IHardwareAdapter> jtag() {
    return std::make_unique<MockHardwareAdapter>(DeviceInfo{"segger-jlink-edu-mini", DeviceType::JtagAdapter, "SEGGER", "J-Link EDU Mini", 5.0, 3.3, {"USB 2.0 Full-Speed", "JTAG", "SWD", "SWO"}, {"GDB server", "JTAG max 4 MHz", "SWD", "Target voltage sensing"}, "USB 2.0 / 9-pin 0.05 in Cortex-M Samtec", "Target VIF 1.8 V or 3.3 V; target voltage is sensed, not supplied", ConnectionState::Disconnected, true, ""});
}

} // namespace

std::string to_string(DeviceType type) {
    switch (type) { case DeviceType::Microcontroller: return "microcontroller"; case DeviceType::Fpga: return "fpga"; case DeviceType::JtagAdapter: return "jtag_adapter"; }
    return "unknown";
}

std::string to_string(ConnectionState state) {
    switch (state) { case ConnectionState::Discovered: return "discovered"; case ConnectionState::Connected: return "connected"; case ConnectionState::Disconnected: return "disconnected"; case ConnectionState::Fault: return "fault"; }
    return "unknown";
}

HardwareManager::HardwareManager() { adapters_.push_back(microcontroller()); adapters_.push_back(fpga()); adapters_.push_back(jtag()); }

IHardwareAdapter& HardwareManager::adapter(const std::string& device_id) {
    for (auto& candidate : adapters_) if (candidate->capabilities().device_id == device_id) return *candidate;
    throw std::invalid_argument("Unknown device_id: " + device_id);
}
const IHardwareAdapter& HardwareManager::adapter(const std::string& device_id) const {
    for (const auto& candidate : adapters_) if (candidate->capabilities().device_id == device_id) return *candidate;
    throw std::invalid_argument("Unknown device_id: " + device_id);
}

void HardwareManager::record(const std::string& device_id, const std::string& level, const std::string& message) {
    const auto& info = adapter(device_id).capabilities();
    events_.push_back({++sequence_, device_id, level, message, info.state, info.io_voltage_V, info.state == ConnectionState::Connected ? 29.5 : 24.0, true});
    if (events_.size() > 100) events_.erase(events_.begin());
}

std::vector<DeviceInfo> HardwareManager::discover() {
    std::lock_guard<std::mutex> lock(mutex_); std::vector<DeviceInfo> result;
    for (auto& candidate : adapters_) { result.push_back(candidate->discover()); record(candidate->capabilities().device_id, "info", "Simulated device discovery complete"); }
    return result;
}
DeviceInfo HardwareManager::connect(const std::string& device_id) { std::lock_guard<std::mutex> lock(mutex_); auto& candidate = adapter(device_id); candidate.connect(); record(device_id, "info", "Simulated connection established"); return candidate.capabilities(); }
DeviceInfo HardwareManager::disconnect(const std::string& device_id) { std::lock_guard<std::mutex> lock(mutex_); auto& candidate = adapter(device_id); candidate.disconnect(); record(device_id, "info", "Simulated device disconnected"); return candidate.capabilities(); }
DeviceInfo HardwareManager::simulate_failure(const std::string& device_id) { std::lock_guard<std::mutex> lock(mutex_); auto& candidate = adapter(device_id); candidate.simulate_timeout(); record(device_id, "error", "Simulated connection timeout detected"); return candidate.capabilities(); }
DeviceInfo HardwareManager::recover(const std::string& device_id) { std::lock_guard<std::mutex> lock(mutex_); auto& candidate = adapter(device_id); candidate.recover(); record(device_id, "info", "Recovery action completed; simulated reconnection established"); return candidate.capabilities(); }
std::vector<DeviceInfo> HardwareManager::devices() const { std::lock_guard<std::mutex> lock(mutex_); std::vector<DeviceInfo> result; for (const auto& candidate : adapters_) result.push_back(candidate->capabilities()); return result; }
std::vector<TelemetryMessage> HardwareManager::telemetry() const { std::lock_guard<std::mutex> lock(mutex_); auto result = events_; std::size_t next_sequence = sequence_ + 1; for (const auto& candidate : adapters_) result.push_back(candidate->telemetry(next_sequence++)); return result; }

} // namespace nexus::hardware
