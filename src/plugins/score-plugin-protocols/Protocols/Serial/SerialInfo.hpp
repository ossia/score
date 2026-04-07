// Ported from Qt 6 QSerialPortInfo. Original copyrights:
// Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
// Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <score_plugin_protocols_export.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace serial {

struct port_info {
    std::string port_name;       // e.g. "ttyUSB0", "COM3", "cu.usbserial-1410"
    std::string system_location; // e.g. "/dev/ttyUSB0", "\\\\.\\COM3"
    std::string description;
    std::string manufacturer;
    std::string serial_number;

    std::optional<uint16_t> vendor_id;
    std::optional<uint16_t> product_id;
};

// Enumerate all serial ports currently available on the system.
// The returned system_location strings are directly usable with
// boost::asio::serial_port(io_context, system_location).
SCORE_PLUGIN_PROTOCOLS_EXPORT
std::vector<port_info> available_ports();
SCORE_PLUGIN_PROTOCOLS_EXPORT
std::vector<int32_t> standard_baud_rates();
}