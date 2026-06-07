#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace telemetry {

enum class ByteOrder {
    BigEndian,
    LittleEndian
};

struct SignalDefinition {
    std::string name;
    std::uint16_t start_bit{};
    std::uint16_t bit_length{};
    ByteOrder byte_order{ByteOrder::LittleEndian};
    bool is_signed{};
    double factor{1.0};
    double offset{0.0};
    double minimum{};
    double maximum{};
    std::string unit;
};

struct MessageDefinition {
    std::uint32_t frame_id{};
    std::string name;
    std::uint8_t dlc{};
    std::string transmitter;
    std::vector<SignalDefinition> signals;
};

class DbcDatabase {
public:
    void add_message(MessageDefinition message);
    [[nodiscard]] const MessageDefinition* find_message(std::uint32_t frame_id) const;
    [[nodiscard]] const std::unordered_map<std::uint32_t, MessageDefinition>& messages() const;

private:
    std::unordered_map<std::uint32_t, MessageDefinition> messages_;
};

class DbcParser {
public:
    [[nodiscard]] static DbcDatabase parse_file(const std::string& path);
    [[nodiscard]] static DbcDatabase parse_string(const std::string& dbc_text);
};

}  // namespace telemetry
