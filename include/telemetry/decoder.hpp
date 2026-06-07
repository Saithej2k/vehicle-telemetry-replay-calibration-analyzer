#pragma once

#include "telemetry/can_frame.hpp"
#include "telemetry/dbc.hpp"

#include <string>
#include <vector>

namespace telemetry {

struct DecodedSignal {
    std::uint64_t timestamp_ns{};
    std::uint32_t frame_id{};
    std::string message;
    std::string signal;
    double value{};
    std::string unit;
    std::optional<std::uint64_t> sequence;
};

class SignalDecoder {
public:
    explicit SignalDecoder(const DbcDatabase& database);

    [[nodiscard]] std::vector<DecodedSignal> decode(const CanFrame& frame) const;

private:
    const DbcDatabase& database_;
};

}  // namespace telemetry
