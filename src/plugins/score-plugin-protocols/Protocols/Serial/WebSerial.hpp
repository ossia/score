#pragma once
#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_SERIAL) && defined(__EMSCRIPTEN__)
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Protocols::WebSerial
{
struct PortInfo
{
  std::string id;
  std::optional<uint16_t> vendor_id;
  std::optional<uint16_t> product_id;

  bool operator==(const PortInfo& other) const noexcept = default;
};

enum class Activation
{
  Unsupported = -1,
  Inactive = 0,
  Active = 1
};

bool available() noexcept;

Activation userActivation() noexcept;

//! Asynchronous navigator.serial.getPorts()
void scan() noexcept;

//! Incremented every time the granted-port list changes
int generation() noexcept;

//! Reads the browser-side port list into the cache. Main thread only.
void refresh() noexcept;

//! Last refreshed port list ; callable from any thread.
std::vector<PortInfo> cachedPorts() noexcept;

//! navigator.serial.requestPort(). Requires transient user activation: if there
//! is none left when this runs, the request is armed and fires on the next click.
void requestPort() noexcept;

//! True between requestPort() and the browser chooser actually opening.
bool requestPending() noexcept;

//! Returns a handle > 0, or 0. The port is not usable until status() is 1.
int open(const std::string& id, int baudRate) noexcept;

//! 0: opening, 1: open, -1: failed
int status(int handle) noexcept;

std::string error(int handle) noexcept;

int read(int handle, char* buf, int capacity) noexcept;

void write(int handle, const char* data, int size) noexcept;

void close(int handle) noexcept;
}
#endif
