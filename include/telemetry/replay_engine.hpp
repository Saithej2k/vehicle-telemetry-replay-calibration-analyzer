#pragma once

#include "telemetry/can_frame.hpp"
#include "telemetry/csv.hpp"
#include "telemetry/dbc.hpp"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

namespace telemetry {

struct ReplayOptions {
    std::string input_csv;
    std::string dbc_file;
    std::string output_jsonl;
    std::string summary_json;
    std::string socketcan_interface;
    double speed{1.0};
    bool no_sleep{false};
    std::uint64_t max_sleep_us{250000};
};

struct ReplayStats {
    std::uint64_t frames{};
    std::uint64_t decoded_signals{};
    std::uint64_t duplicates{};
    std::uint64_t out_of_order{};
    std::uint64_t dropped_frames{};
    std::uint64_t unknown_messages{};
    std::optional<std::uint64_t> first_timestamp_ns;
    std::optional<std::uint64_t> last_timestamp_ns;
};

class ReplayEngine {
public:
    explicit ReplayEngine(ReplayOptions options);

    [[nodiscard]] ReplayStats run();

private:
    ReplayOptions options_;
};

std::string replay_stats_json(const ReplayStats& stats);
CanFrame frame_from_csv_row(const CsvRow& row);

}  // namespace telemetry
