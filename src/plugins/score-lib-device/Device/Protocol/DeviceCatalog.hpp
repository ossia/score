#pragma once
#include <Device/Protocol/DeviceSettings.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

#include <score_lib_device_export.h>

#include <functional>
#include <vector>

namespace Device
{
class ProtocolFactory;

/**
 * @brief What can be added to a document, and where the hardware is.
 *
 * "Add a device" has always meant this machine's protocols and this machine's
 * hardware, because the score ran here. For a score that runs somewhere else it
 * has to mean that machine's: its MIDI ports, its cameras, its protocols.
 * Offering ours would offer something the score can never reach.
 *
 * Asynchronous for the same reason score::Environment is: the answer may have
 * to come over a socket, and an interface that let callers wait would be one
 * only the local implementation could satisfy. The dialog already fills its
 * list from signals as enumerators discover things, so this fits how it works.
 */
class SCORE_LIB_DEVICE_EXPORT DeviceCatalog
{
public:
  virtual ~DeviceCatalog();

  struct Protocol
  {
    UuidKey<Device::ProtocolFactory> key;
    QString name;
    QString category;

    //! Whether this build has the factory. When it does not, there is no
    //! settings widget to show -- the widget is C++ in a plug-in we do not
    //! have -- so such a protocol can only be used through what it enumerates.
    bool constructible{};
  };

  //! (category, name, settings). The category is the enumerator's -- "Cameras",
  //! "Screens" -- and is what the list groups by, so it stays a field rather
  //! than being folded into the name.
  using OnDevice = std::function<void(
      const QString& category, const QString& name, const Device::DeviceSettings&)>;

  virtual std::vector<Protocol> protocols() const = 0;

  //! Hardware currently present, for one protocol. The callback may be invoked
  //! any number of times, and later than this returns.
  virtual void enumerate(const UuidKey<Device::ProtocolFactory>& protocol, OnDevice) = 0;
};
}
