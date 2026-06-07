#pragma once

#include "telemetry/can_frame.hpp"

#include <string>

namespace telemetry {

class SocketCanWriter {
public:
    SocketCanWriter();
    ~SocketCanWriter();

    SocketCanWriter(const SocketCanWriter&) = delete;
    SocketCanWriter& operator=(const SocketCanWriter&) = delete;

    void open(const std::string& interface_name);
    void write(const CanFrame& frame);
    [[nodiscard]] bool is_open() const;

private:
    int fd_{-1};
};

}  // namespace telemetry
