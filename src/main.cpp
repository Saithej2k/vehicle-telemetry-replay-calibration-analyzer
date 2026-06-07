#include "telemetry/replay_engine.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage(std::ostream& output) {
    output << "Usage: telemetry_replay --dbc FILE --input CSV [options]\n\n"
           << "Options:\n"
           << "  --output FILE          Write decoded JSON Lines to FILE instead of stdout\n"
           << "  --summary FILE         Write replay integrity summary JSON\n"
           << "  --socketcan IFACE      Also transmit raw frames on a SocketCAN interface\n"
           << "  --speed N              Replay speed multiplier, default 1.0\n"
           << "  --max-sleep-us N       Cap per-frame sleep, default 250000\n"
           << "  --no-sleep             Decode immediately while preserving timestamps in output\n"
           << "  --help                 Show this help text\n";
}

std::string require_value(int& index, int argc, char** argv, const std::string& flag) {
    if (index + 1 >= argc) {
        throw std::runtime_error(flag + " requires a value");
    }
    return argv[++index];
}

}  // namespace

int main(int argc, char** argv) {
    telemetry::ReplayOptions options;

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--help") {
                print_usage(std::cout);
                return 0;
            }
            if (arg == "--dbc") {
                options.dbc_file = require_value(index, argc, argv, arg);
            } else if (arg == "--input") {
                options.input_csv = require_value(index, argc, argv, arg);
            } else if (arg == "--output") {
                options.output_jsonl = require_value(index, argc, argv, arg);
            } else if (arg == "--summary") {
                options.summary_json = require_value(index, argc, argv, arg);
            } else if (arg == "--socketcan") {
                options.socketcan_interface = require_value(index, argc, argv, arg);
            } else if (arg == "--speed") {
                options.speed = std::stod(require_value(index, argc, argv, arg));
            } else if (arg == "--max-sleep-us") {
                options.max_sleep_us = static_cast<std::uint64_t>(
                    std::stoull(require_value(index, argc, argv, arg)));
            } else if (arg == "--no-sleep") {
                options.no_sleep = true;
            } else {
                throw std::runtime_error("unknown argument: " + arg);
            }
        }

        if (options.dbc_file.empty() || options.input_csv.empty()) {
            print_usage(std::cerr);
            return 2;
        }

        const auto stats = telemetry::ReplayEngine(options).run();
        std::cerr << "frames=" << stats.frames
                  << " decoded_signals=" << stats.decoded_signals
                  << " duplicates=" << stats.duplicates
                  << " out_of_order=" << stats.out_of_order
                  << " dropped_frames=" << stats.dropped_frames
                  << " unknown_messages=" << stats.unknown_messages << '\n';
    } catch (const std::exception& error) {
        std::cerr << "telemetry_replay: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
