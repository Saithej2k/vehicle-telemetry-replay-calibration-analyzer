#include "telemetry/can_frame.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace telemetry {
namespace {

std::string compact_hex(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
                    return c == ' ' || c == ':' || c == '-' || c == '_';
                }),
                value.end());
    if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
        value.erase(0, 2);
    }
    return value;
}

}  // namespace

std::string CanFrame::payload_hex() const {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < dlc; ++index) {
        output << std::setw(2) << static_cast<int>(data[index]);
    }
    return output.str();
}

std::vector<std::uint8_t> parse_payload_hex(const std::string& payload) {
    const std::string hex = compact_hex(payload);
    if (hex.empty()) {
        return {};
    }
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("payload_hex must contain an even number of hex digits");
    }
    if (hex.size() / 2 > 8) {
        throw std::runtime_error("CAN payloads are limited to 8 bytes");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        unsigned value = 0;
        const auto* first = hex.data() + index;
        const auto* last = first + 2;
        const auto parsed = std::from_chars(first, last, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != last) {
            throw std::runtime_error("payload_hex contains a non-hex byte");
        }
        bytes.push_back(static_cast<std::uint8_t>(value));
    }
    return bytes;
}

std::uint32_t parse_frame_id(const std::string& value) {
    std::string text = value;
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
                   return c == ' ' || c == '\t';
               }),
               text.end());
    int base = 10;
    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
        text.erase(0, 2);
        base = 16;
    } else if (text.find_first_of("abcdefABCDEF") != std::string::npos) {
        base = 16;
    }

    unsigned long parsed = 0;
    try {
        parsed = std::stoul(text, nullptr, base);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid CAN frame_id: " + value);
    }
    if (parsed > 0x1FFFFFFFUL) {
        throw std::runtime_error("CAN frame_id exceeds 29-bit identifier range");
    }
    return static_cast<std::uint32_t>(parsed);
}

}  // namespace telemetry
