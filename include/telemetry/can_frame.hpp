#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace telemetry {

struct CanFrame {
    std::uint64_t timestamp_ns{};
    std::uint32_t frame_id{};
    std::array<std::uint8_t, 8> data{};
    std::size_t dlc{};
    std::optional<std::uint64_t> sequence;

    [[nodiscard]] std::string payload_hex() const;
};

std::vector<std::uint8_t> parse_payload_hex(const std::string& payload);
std::uint32_t parse_frame_id(const std::string& value);

}  // namespace telemetry
