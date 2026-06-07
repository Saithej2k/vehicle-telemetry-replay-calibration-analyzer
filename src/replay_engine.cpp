#include "telemetry/replay_engine.hpp"

#include "telemetry/decoder.hpp"
#include "telemetry/socketcan.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace telemetry {
namespace {

std::uint64_t parse_u64(const std::string& value, const std::string& field_name) {
    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw std::runtime_error("invalid " + field_name + ": " + value);
    }
}

std::string required(const CsvRow& row, const std::string& key) {
    const auto iterator = row.find(key);
    if (iterator == row.end() || trim(iterator->second).empty()) {
        throw std::runtime_error("CSV row is missing required column: " + key);
    }
    return iterator->second;
}

std::optional<std::string> optional_cell(const CsvRow& row, const std::string& key) {
    const auto iterator = row.find(key);
    if (iterator == row.end() || trim(iterator->second).empty()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::string json_escape(const std::string& value) {
    std::ostringstream escaped;
    for (const char c : value) {
        switch (c) {
            case '\\':
                escaped << "\\\\";
                break;
            case '"':
                escaped << "\\\"";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                escaped << c;
                break;
        }
    }
    return escaped.str();
}

std::uint64_t frame_key(const CanFrame& frame) {
    std::uint64_t hash = 1469598103934665603ULL;
    auto mix = [&hash](std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            hash ^= static_cast<std::uint8_t>((value >> shift) & 0xFFU);
            hash *= 1099511628211ULL;
        }
    };
    mix(frame.timestamp_ns);
    mix(frame.frame_id);
    for (std::size_t index = 0; index < frame.dlc; ++index) {
        hash ^= frame.data[index];
        hash *= 1099511628211ULL;
    }
    if (frame.sequence.has_value()) {
        mix(*frame.sequence);
    }
    return hash;
}

void write_signal_json(std::ostream& output, const DecodedSignal& signal) {
    output << "{\"timestamp_ns\":" << signal.timestamp_ns
           << ",\"frame_id\":" << signal.frame_id
           << ",\"message\":\"" << json_escape(signal.message)
           << "\",\"signal\":\"" << json_escape(signal.signal)
           << "\",\"value\":" << std::setprecision(12) << signal.value
           << ",\"unit\":\"" << json_escape(signal.unit) << "\"";
    if (signal.sequence.has_value()) {
        output << ",\"sequence\":" << *signal.sequence;
    }
    output << "}\n";
}

void preserve_timestamp_delta(
    std::optional<std::uint64_t> previous_timestamp_ns,
    std::uint64_t timestamp_ns,
    const ReplayOptions& options) {
    if (options.no_sleep || !previous_timestamp_ns.has_value() || timestamp_ns <= *previous_timestamp_ns) {
        return;
    }
    const double speed = options.speed <= 0.0 ? 1.0 : options.speed;
    const auto delta_ns = static_cast<double>(timestamp_ns - *previous_timestamp_ns);
    const auto requested_us = static_cast<std::uint64_t>((delta_ns / 1000.0) / speed);
    const auto sleep_us = std::min(requested_us, options.max_sleep_us);
    if (sleep_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
    }
}

}  // namespace

ReplayEngine::ReplayEngine(ReplayOptions options) : options_(std::move(options)) {}

CanFrame frame_from_csv_row(const CsvRow& row) {
    CanFrame frame;
    frame.timestamp_ns = parse_u64(required(row, "timestamp_ns"), "timestamp_ns");
    frame.frame_id = parse_frame_id(required(row, "frame_id"));

    const auto payload = optional_cell(row, "payload_hex").value_or(
        optional_cell(row, "data").value_or(""));
    if (payload.empty()) {
        throw std::runtime_error("CSV row is missing required column: payload_hex");
    }
    const auto bytes = parse_payload_hex(payload);
    frame.dlc = bytes.size();
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        frame.data[index] = bytes[index];
    }

    if (const auto sequence = optional_cell(row, "sequence"); sequence.has_value()) {
        frame.sequence = parse_u64(*sequence, "sequence");
    }
    return frame;
}

ReplayStats ReplayEngine::run() {
    const auto database = DbcParser::parse_file(options_.dbc_file);
    const SignalDecoder decoder(database);

    std::ifstream input(options_.input_csv);
    if (!input) {
        throw std::runtime_error("failed to open replay input: " + options_.input_csv);
    }
    CsvReader reader(input);

    std::unique_ptr<std::ofstream> output_file;
    std::ostream* output = &std::cout;
    if (!options_.output_jsonl.empty()) {
        output_file = std::make_unique<std::ofstream>(options_.output_jsonl);
        if (!*output_file) {
            throw std::runtime_error("failed to open replay output: " + options_.output_jsonl);
        }
        output = output_file.get();
    }

    SocketCanWriter socketcan;
    if (!options_.socketcan_interface.empty()) {
        socketcan.open(options_.socketcan_interface);
    }

    ReplayStats stats;
    std::optional<std::uint64_t> previous_timestamp_ns;
    std::unordered_set<std::uint64_t> seen_frames;
    std::unordered_map<std::uint32_t, std::uint64_t> last_sequence_by_id;
    CsvRow row;

    while (reader.read(row)) {
        const CanFrame frame = frame_from_csv_row(row);
        if (!stats.first_timestamp_ns.has_value()) {
            stats.first_timestamp_ns = frame.timestamp_ns;
        }
        stats.last_timestamp_ns = frame.timestamp_ns;

        if (previous_timestamp_ns.has_value() && frame.timestamp_ns < *previous_timestamp_ns) {
            ++stats.out_of_order;
        }
        if (!seen_frames.insert(frame_key(frame)).second) {
            ++stats.duplicates;
        }
        if (frame.sequence.has_value()) {
            const auto iterator = last_sequence_by_id.find(frame.frame_id);
            if (iterator != last_sequence_by_id.end() && *frame.sequence > iterator->second + 1) {
                stats.dropped_frames += *frame.sequence - iterator->second - 1;
            }
            last_sequence_by_id[frame.frame_id] = *frame.sequence;
        }

        preserve_timestamp_delta(previous_timestamp_ns, frame.timestamp_ns, options_);
        previous_timestamp_ns = frame.timestamp_ns;

        if (socketcan.is_open()) {
            socketcan.write(frame);
        }

        const auto decoded = decoder.decode(frame);
        if (decoded.empty()) {
            ++stats.unknown_messages;
        }
        for (const auto& signal : decoded) {
            write_signal_json(*output, signal);
            ++stats.decoded_signals;
        }
        ++stats.frames;
    }

    if (!options_.summary_json.empty()) {
        std::ofstream summary(options_.summary_json);
        if (!summary) {
            throw std::runtime_error("failed to open replay summary: " + options_.summary_json);
        }
        summary << replay_stats_json(stats) << '\n';
    }

    return stats;
}

std::string replay_stats_json(const ReplayStats& stats) {
    std::ostringstream output;
    output << "{\n"
           << "  \"frames\": " << stats.frames << ",\n"
           << "  \"decoded_signals\": " << stats.decoded_signals << ",\n"
           << "  \"duplicates\": " << stats.duplicates << ",\n"
           << "  \"out_of_order\": " << stats.out_of_order << ",\n"
           << "  \"dropped_frames\": " << stats.dropped_frames << ",\n"
           << "  \"unknown_messages\": " << stats.unknown_messages << ",\n"
           << "  \"first_timestamp_ns\": ";
    if (stats.first_timestamp_ns.has_value()) {
        output << *stats.first_timestamp_ns;
    } else {
        output << "null";
    }
    output << ",\n  \"last_timestamp_ns\": ";
    if (stats.last_timestamp_ns.has_value()) {
        output << *stats.last_timestamp_ns;
    } else {
        output << "null";
    }
    output << "\n}";
    return output.str();
}

}  // namespace telemetry
