#include "telemetry/can_frame.hpp"
#include "telemetry/csv.hpp"
#include "telemetry/dbc.hpp"
#include "telemetry/decoder.hpp"
#include "telemetry/replay_engine.hpp"

#include <stdexcept>
#include <sstream>

namespace {

const char* kDbc = R"DBC(
BO_ 291 STEERING: 8 Vector__XXX
 SG_ steering_angle_deg : 0|16@1- (0.1,-780) [-780|780] "deg" Vector__XXX
 SG_ steering_rate_deg_s : 16|16@1- (0.5,-1000) [-1000|1000] "deg_s" Vector__XXX
 SG_ steering_seq : 48|8@1+ (1,0) [0|255] "" Vector__XXX
BO_ 292 VEHICLE_SPEED: 8 Vector__XXX
 SG_ vehicle_speed_mps : 0|16@1+ (0.01,0) [0|100] "m_s" Vector__XXX
)DBC";

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_payload_parsing() {
    const auto payload = telemetry::parse_payload_hex("DC 1E D0 07 00 00 03 00");
    require(payload.size() == 8, "payload size");
    require(payload[0] == 0xDC, "payload byte 0");
    require(payload[6] == 0x03, "payload byte 6");
    require(telemetry::parse_frame_id("0x123") == 0x123, "frame id");
}

void test_dbc_decode() {
    const auto database = telemetry::DbcParser::parse_string(kDbc);
    const auto* message = database.find_message(291);
    require(message != nullptr, "message parsed");
    require(message->signals.size() == 3, "signals parsed");

    telemetry::CanFrame frame;
    frame.timestamp_ns = 1000000;
    frame.frame_id = 291;
    frame.dlc = 8;
    const auto bytes = telemetry::parse_payload_hex("DC1ED00700000300");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        frame.data[index] = bytes[index];
    }
    frame.sequence = 3;

    const telemetry::SignalDecoder decoder(database);
    const auto decoded = decoder.decode(frame);
    require(decoded.size() == 3, "decoded count");
    require(decoded[0].signal == "steering_angle_deg", "decoded signal name");
    require(decoded[0].value > 9.9 && decoded[0].value < 10.1, "decoded steering angle");
    require(decoded[1].value > -0.1 && decoded[1].value < 0.1, "decoded steering rate");
    require(decoded[2].value == 3.0, "decoded sequence");
}

void test_csv_and_frame_mapping() {
    std::istringstream input("timestamp_ns,frame_id,payload_hex,sequence\n100,0x123,010203,7\n");
    telemetry::CsvReader reader(input);
    telemetry::CsvRow row;
    require(reader.read(row), "csv row read");
    const auto frame = telemetry::frame_from_csv_row(row);
    require(frame.timestamp_ns == 100, "csv timestamp");
    require(frame.frame_id == 0x123, "csv frame id");
    require(frame.dlc == 3, "csv dlc");
    require(frame.sequence.has_value() && *frame.sequence == 7, "csv sequence");
}

}  // namespace

int main() {
    test_payload_parsing();
    test_dbc_decode();
    test_csv_and_frame_mapping();
    return 0;
}
