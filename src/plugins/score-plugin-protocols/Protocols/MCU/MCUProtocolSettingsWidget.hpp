#pragma once
#include <Protocols/MCU/MCUSpecificSettings.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/protocols/midi/midi.hpp>

#include <libremidi/api.hpp>

#include <QString>

#include <memory>
#include <utility>
#include <vector>

#include <verdigris>

#include <libremidi/port_information.hpp>

namespace libremidi
{
class observer;
}
namespace State
{
class AddressFragmentLineEdit;
}
namespace score
{
class ComboBox;
}
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QSortFilterProxyModel;
class QSpinBox;
class QStandardItemModel;
class QTreeView;
class QWidget;

namespace Protocols
{
/**
 * Settings for the MIDI Controller device: a Mackie Control surface, which
 * needs only its two ports, or a MIDI Guide instrument, which also needs
 * picking out of a few hundred and a channel. The picker is a filtered tree
 * because that many instruments do not fit a drop-down.
 */
class MCUSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(MCUSettingsWidget)

public:
  explicit MCUSettingsWidget(QWidget* parent = nullptr);
  ~MCUSettingsWidget();

private:
  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

  //! Show or hide the instrument picker.
  void updateKind();

  //! A name for the hardware the chosen ports belong to, empty when none is
  //! chosen.
  QString nameFromPorts() const;

  //! Rename the device, unless the user has named it themselves.
  void applyAutoName(const QString& name);

  static QString defaultName();

  //! Name the device after the chosen description, or the port when none is
  //! chosen.
  void refreshAutoName();

  //! Fill the picker from the installed device maps.
  void populateDeviceMaps();
  QString selectedMap() const;

  //! What the user picked, which survives the search box filtering it away.
  QString chosenMap() const;

  //! The devices on the port, as the list holds them.
  MCUSpecificSettings::MapSlot slotAt(int row) const;
  QString labelForSlot(const MCUSpecificSettings::MapSlot& slot) const;
  void addChosenDevice(const QString& identity, int channel);
  void selectMap(const QString& identity);
  void updateDeviceMapSummary();

  //! Add a port to its combo box, unless already listed.
  void addInput(const libremidi::input_port& p);
  void addOutput(const libremidi::output_port& p);

  //! Drop a port that has gone away.
  template <typename Port>
  void
  removePort(QComboBox& combo, const std::vector<Port>& ports, const Port& gone);

  State::AddressFragmentLineEdit* m_name{};
  QComboBox* m_kind{};
  QComboBox* m_midiin{};
  QComboBox* m_midiout{};

  //! Everything that only concerns an instrument, shown and hidden together.
  QWidget* m_instrumentBox{};
  QLineEdit* m_search{};
  QTreeView* m_instruments{};
  QStandardItemModel* m_instrumentModel{};
  QSortFilterProxyModel* m_instrumentFilter{};
  QSpinBox* m_channel{};
  QListWidget* m_chosen{};
  QLabel* m_summary{};

  Device::DeviceSettings m_current;

  //! Which library the picker currently holds, so that switching kinds
  //! refills it and staying on one does not.
  int m_populated{-1};

  //! @see chosenMap()
  QString m_chosenMap;

  //! The name this widget last filled in by itself: picking an instrument
  //! renames the device only while the user has not named it.
  QString m_autoName;

  //! The backend the port lists came from, and the one the device is opened
  //! with.
  libremidi::API m_api{};

  //! Were the saved ports found among the live ones? Distinguishes "absent"
  //! from "the user chose none".
  bool m_portsResolved{};

  std::unique_ptr<libremidi::observer> m_observer{};

  std::vector<libremidi::input_port> m_ins;
  std::vector<libremidi::output_port> m_outs;
};
}
