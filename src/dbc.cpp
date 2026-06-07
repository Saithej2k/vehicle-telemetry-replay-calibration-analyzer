#include "telemetry/dbc.hpp"

#include "telemetry/csv.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace telemetry {

void DbcDatabase::add_message(MessageDefinition message) {
    messages_[message.frame_id] = std::move(message);
}

const MessageDefinition* DbcDatabase::find_message(std::uint32_t frame_id) const {
    const auto iterator = messages_.find(frame_id);
    return iterator == messages_.end() ? nullptr : &iterator->second;
}

const std::unordered_map<std::uint32_t, MessageDefinition>& DbcDatabase::messages() const {
    return messages_;
}

DbcDatabase DbcParser::parse_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open DBC file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_string(buffer.str());
}

DbcDatabase DbcParser::parse_string(const std::string& dbc_text) {
    DbcDatabase database;
    std::istringstream input(dbc_text);
    std::string line;

    const std::regex message_pattern(R"(^BO_\s+(\d+)\s+([A-Za-z0-9_]+)\s*:\s*(\d+)\s+(\S+))");
    const std::regex signal_pattern(
        R"DBC(^SG_\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s+\S+)?\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(([-+0-9.eE]+),([-+0-9.eE]+)\)\s*\[([-+0-9.eE]+)\|([-+0-9.eE]+)\]\s*"([^"]*)")DBC");

    MessageDefinition current;
    bool has_current = false;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::smatch match;
        if (std::regex_search(line, match, message_pattern)) {
            if (has_current) {
                database.add_message(std::move(current));
                current = MessageDefinition{};
            }
            current.frame_id = static_cast<std::uint32_t>(std::stoul(match[1].str()));
            current.name = match[2].str();
            current.dlc = static_cast<std::uint8_t>(std::stoul(match[3].str()));
            current.transmitter = match[4].str();
            has_current = true;
            continue;
        }

        if (has_current && std::regex_search(line, match, signal_pattern)) {
            SignalDefinition signal;
            signal.name = match[1].str();
            signal.start_bit = static_cast<std::uint16_t>(std::stoul(match[2].str()));
            signal.bit_length = static_cast<std::uint16_t>(std::stoul(match[3].str()));
            signal.byte_order = match[4].str() == "1" ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
            signal.is_signed = match[5].str() == "-";
            signal.factor = std::stod(match[6].str());
            signal.offset = std::stod(match[7].str());
            signal.minimum = std::stod(match[8].str());
            signal.maximum = std::stod(match[9].str());
            signal.unit = match[10].str();
            current.signals.push_back(std::move(signal));
        }
    }

    if (has_current) {
        database.add_message(std::move(current));
    }
    return database;
}

}  // namespace telemetry
