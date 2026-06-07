#include "telemetry/decoder.hpp"

#include <cmath>
#include <cstdint>

namespace telemetry {
namespace {

std::uint64_t extract_little_endian(const CanFrame& frame, const SignalDefinition& signal) {
    std::uint64_t raw = 0;
    for (std::uint16_t offset = 0; offset < signal.bit_length; ++offset) {
        const std::uint16_t source_bit = signal.start_bit + offset;
        const std::size_t byte_index = source_bit / 8;
        const std::uint8_t bit_index = source_bit % 8;
        if (byte_index < frame.dlc && ((frame.data[byte_index] >> bit_index) & 0x01U) != 0U) {
            raw |= (1ULL << offset);
        }
    }
    return raw;
}

std::uint64_t extract_big_endian(const CanFrame& frame, const SignalDefinition& signal) {
    std::uint64_t raw = 0;
    int source_bit = signal.start_bit;
    for (std::uint16_t offset = 0; offset < signal.bit_length; ++offset) {
        const std::size_t byte_index = static_cast<std::size_t>(source_bit / 8);
        const std::uint8_t bit_index = static_cast<std::uint8_t>(source_bit % 8);
        raw <<= 1U;
        if (byte_index < frame.dlc && ((frame.data[byte_index] >> bit_index) & 0x01U) != 0U) {
            raw |= 1U;
        }
        source_bit = (source_bit % 8 == 0) ? source_bit + 15 : source_bit - 1;
    }
    return raw;
}

std::int64_t sign_extend(std::uint64_t raw, std::uint16_t bit_length) {
    if (bit_length == 0 || bit_length >= 64) {
        return static_cast<std::int64_t>(raw);
    }
    const std::uint64_t sign_bit = 1ULL << (bit_length - 1);
    if ((raw & sign_bit) == 0) {
        return static_cast<std::int64_t>(raw);
    }
    const std::uint64_t mask = ~((1ULL << bit_length) - 1);
    return static_cast<std::int64_t>(raw | mask);
}

}  // namespace

SignalDecoder::SignalDecoder(const DbcDatabase& database) : database_(database) {}

std::vector<DecodedSignal> SignalDecoder::decode(const CanFrame& frame) const {
    std::vector<DecodedSignal> decoded;
    const auto* message = database_.find_message(frame.frame_id);
    if (message == nullptr) {
        return decoded;
    }

    decoded.reserve(message->signals.size());
    for (const auto& signal : message->signals) {
        if (signal.bit_length == 0 || signal.bit_length > 64) {
            continue;
        }
        const std::uint64_t raw = signal.byte_order == ByteOrder::LittleEndian
                                      ? extract_little_endian(frame, signal)
                                      : extract_big_endian(frame, signal);
        const double scaled = (signal.is_signed ? static_cast<double>(sign_extend(raw, signal.bit_length))
                                                : static_cast<double>(raw)) *
                                  signal.factor +
                              signal.offset;
        decoded.push_back(DecodedSignal{
            frame.timestamp_ns,
            frame.frame_id,
            message->name,
            signal.name,
            scaled,
            signal.unit,
            frame.sequence,
        });
    }

    return decoded;
}

}  // namespace telemetry
