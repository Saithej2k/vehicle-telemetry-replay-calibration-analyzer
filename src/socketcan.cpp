#include "telemetry/socketcan.hpp"

#include <algorithm>
#include <stdexcept>

#ifdef __linux__
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace telemetry {

SocketCanWriter::SocketCanWriter() = default;

SocketCanWriter::~SocketCanWriter() {
#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
    }
#endif
}

void SocketCanWriter::open(const std::string& interface_name) {
    if (interface_name.empty()) {
        return;
    }
#ifdef __linux__
    fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) {
        throw std::runtime_error("failed to create SocketCAN socket");
    }

    ifreq request {};
    std::strncpy(request.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd_, SIOCGIFINDEX, &request) < 0) {
        throw std::runtime_error("failed to resolve SocketCAN interface: " + interface_name);
    }

    sockaddr_can address {};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("failed to bind SocketCAN interface: " + interface_name);
    }
#else
    (void)interface_name;
    throw std::runtime_error("SocketCAN replay is available on Linux hosts");
#endif
}

void SocketCanWriter::write(const CanFrame& frame) {
    if (fd_ < 0) {
        return;
    }
#ifdef __linux__
    can_frame output {};
    output.can_id = frame.frame_id;
    output.can_dlc = static_cast<__u8>(frame.dlc);
    std::copy(frame.data.begin(), frame.data.begin() + static_cast<std::ptrdiff_t>(frame.dlc), output.data);
    const auto written = ::write(fd_, &output, sizeof(output));
    if (written != static_cast<ssize_t>(sizeof(output))) {
        throw std::runtime_error("failed to write SocketCAN frame");
    }
#else
    (void)frame;
#endif
}

bool SocketCanWriter::is_open() const {
    return fd_ >= 0;
}

}  // namespace telemetry
