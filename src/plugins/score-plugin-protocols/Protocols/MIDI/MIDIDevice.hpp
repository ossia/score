#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <score_plugin_protocols_export.h>

#include <libremidi/api.hpp>
#include <libremidi/configurations.hpp>
#include <libremidi/input_configuration.hpp>
#include <libremidi/output_configuration.hpp>

#include <utility>

namespace libremidi
{
class observer;
}
namespace Protocols
{
class MidiKeyboardEventFilter;
struct MIDISpecificSettings;
class MIDIDevice;

/**
 * @brief Is the MIDI backend @p api present in this build / on this machine?
 *
 * libremidi::available_apis() lists the backends that were COMPILED IN, so a
 * document written on Linux against ALSA_SEQ answers false on a macOS or
 * Windows build and on a Linux build made without ALSA.
 *
 * This must be asked, not asserted: which backends exist is external state.
 */
SCORE_PLUGIN_PROTOCOLS_EXPORT
bool midiApiAvailable(libremidi::API api) noexcept;

/**
 * @brief Build the libremidi input configuration @p set asks for.
 *
 * @p self may be null: it is only needed to install the keyboard event filter
 * for the KEYBOARD backend.
 *
 * Exported rather than file-static because this is the function that used to
 * abort: its per-API branches fetched the backend-specific configuration out of
 * a variant with `SCORE_ASSERT(ptr)`, and
 * libremidi::midi_in_configuration_for() leaves that variant empty for a
 * backend that is not compiled in. It must degrade to the generic
 * configuration instead, whatever API is asked for.
 */
SCORE_PLUGIN_PROTOCOLS_EXPORT
std::pair<libremidi::input_configuration, libremidi::input_api_configuration>
makeInputConfiguration(MIDIDevice* self, MIDISpecificSettings& set);

/// Counterpart of makeInputConfiguration for output devices.
SCORE_PLUGIN_PROTOCOLS_EXPORT
std::pair<libremidi::output_configuration, libremidi::output_api_configuration>
makeOutputConfiguration(MIDIDevice* self, MIDISpecificSettings& set);

class MIDIDevice final : public Device::OwningDeviceInterface
{
public:
  MIDIDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~MIDIDevice();

  bool reconnect() override;

  void disconnect() override;

  QMimeData* mimeData() const override;

  using OwningDeviceInterface::refresh;
  Device::Node refresh() override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

  const ossia::net::network_context_ptr& m_ctx;
  MidiKeyboardEventFilter* m_kbdfilter{};
};
}
