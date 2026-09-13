#pragma once

/**
 * @file The index of `.midimap.json` device descriptions installed in the user
 * library.
 *
 * The maps are shipped as a score package and unpacked as-is:
 *
 *     <library>/packages/midi-device-maps/maps/<source>/<device>.midimap.json
 *
 * `<source>` is where the description was converted from -- `ardour`, `midnam`,
 * `mixxx` -- which is worth keeping because it says what kind of device to
 * expect: a controller's knobs, or an instrument's parameters.
 *
 * Unlike the MIDI Guide index, which reads a device's name off its path, a
 * description names itself. Scanning therefore has to open every file, so it
 * reads a bounded prefix and stops at the first control: an instrument's patch
 * names run to hundreds of kilobytes and a listing must not pay for them.
 * @see Protocols/MIDIDevices/MidiDeviceMap.hpp
 */

#include <Protocols/MIDIDevices/MidiDeviceMap.hpp>

#include <QString>

#include <score_plugin_protocols_export.h>

#include <optional>
#include <vector>

namespace Protocols::MIDIDevices
{

//! Where a description is and what it says about itself, not its controls.
struct DeviceEntry
{
  QString file;

  //! The `maps/<source>` directory it was found in.
  QString source;

  /**
   * "ardour/donnerdmk25.midimap.json": how a score refers to this map.
   * Stored rather than the absolute path, which is not the same on another
   * machine, and rather than the model name, which several documents share --
   * one device has as many maps as it has configurations.
   */
  QString identity;

  DeviceHeader header;

  /**
   * "Akai: MPK 225 (Preset 6)". The preset belongs in the label because the
   * same hardware appears several times over, once per configuration, and the
   * model alone would not tell two of them apart.
   */
  QString label() const;
};

/**
 * The `midi-device-maps` directories of the packages folder: the installed
 * package, and anything the user added under a package of their own. Descends
 * only as far as `maps/<source>`, never the whole packages folder, which is a
 * user directory that can be tens of gigabytes and can contain network mounts.
 */
SCORE_PLUGIN_PROTOCOLS_EXPORT
std::vector<QString> libraryPaths();

/**
 * A process-wide singleton like MidiGuide::Database: read-only, one scan,
 * wanted by every settings widget. Exported so the tests can reach it.
 */
class SCORE_PLUGIN_PROTOCOLS_EXPORT Database
{
public:
  static Database& instance();

  //! Sorted by manufacturer, then model, then preset, case-insensitively.
  const std::vector<DeviceEntry>& devices() const noexcept { return m_devices; }

  //! Already done by the constructor; call it to pick up a later install.
  void rescan();

  //! nullptr when the library has no such map. @see DeviceEntry::identity
  const DeviceEntry* find(const QString& identity) const noexcept;

  //! std::nullopt when the file could not be read or is not a description.
  static std::optional<DeviceMap> load(const DeviceEntry& entry);

private:
  Database();

  std::vector<DeviceEntry> m_devices;
};

}
