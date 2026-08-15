// Device::ToDeviceExplorer: how an ossia network tree is mirrored into the
// device explorer's Device::Node tree, and which of the resulting nodes the
// explorer lets the user edit (Device::Node::isEditable, used by
// DeviceExplorerModel::flags for the Value column).

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// The parameter kinds that carry no user-editable value (MIDI, TEXTURE,
// GEOMETRY) only exist in plug-ins; parameter_type is not settable from the
// outside, so stand in for one here.
struct valueless_parameter final : ossia::net::generic_parameter
{
  explicit valueless_parameter(ossia::net::node_base& node)
      : ossia::net::generic_parameter{node}
  {
    m_type = ossia::parameter_type::MIDI;
  }
};

struct test_device
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "audio"};

  ossia::net::parameter_base& add_audio(std::string_view path)
  {
    auto& node = ossia::net::find_or_create_node(dev.get_root_node(), path);
    node.set_parameter(std::make_unique<ossia::audio_parameter>(node));
    return *node.get_parameter();
  }

  ossia::net::parameter_base& add_message(std::string_view path, ossia::val_type t)
  {
    auto& node = ossia::net::find_or_create_node(dev.get_root_node(), path);
    return *node.create_parameter(t);
  }
};

const Device::Node& child(const Device::Node& n, const QString& name)
{
  auto it = ossia::find_if(n, [&](const Device::Node& c) {
    return c.get<Device::AddressSettings>().name == name;
  });
  REQUIRE(it != n.end());
  return *it;
}
}

TEST_CASE("Message parameters are mirrored and editable", "[device][explorer]")
{
  test_device d;
  d.add_message("/foo", ossia::val_type::INT).set_value(42);

  const auto tree = Device::ToDeviceExplorer(d.dev.get_root_node());
  const auto& foo = child(tree, "foo");

  CHECK(foo.isEditable());
  CHECK(foo.get<Device::AddressSettings>().value == ossia::value{42});
}

// Regression, https://github.com/ossia/score/issues/2204: audio parameters
// carry a plain float gain. Filtering the explorer's tree conversion down to
// parameter_type::MESSAGE left them with no ioType, so Device::Node::isEditable
// returned false and the Value column of the audio device could not be edited.
TEST_CASE("Audio gains are mirrored and editable", "[device][explorer]")
{
  test_device d;
  d.add_audio("/out/1");

  const auto tree = Device::ToDeviceExplorer(d.dev.get_root_node());
  const auto& gain = child(child(tree, "out"), "1");
  const auto& settings = gain.get<Device::AddressSettings>();

  CHECK(gain.isEditable());
  REQUIRE(settings.ioType.has_value());
  CHECK(*settings.ioType == ossia::access_mode::BI);

  // The default gain, as displayed in the Value column.
  REQUIRE(settings.value.get_type() == ossia::val_type::FLOAT);
  CHECK(*settings.value.target<float>() == 1.f);

  // audio_parameter clamps its gain to [0; 1]: advertise that as the domain so
  // that the explorer offers a bounded editor.
  CHECK(ossia::convert<float>(ossia::get_min(settings.domain.get())) == 0.f);
  CHECK(ossia::convert<float>(ossia::get_max(settings.domain.get())) == 1.f);
}

TEST_CASE("Audio gain edits reach the parameter", "[device][explorer]")
{
  test_device d;
  auto& param = d.add_audio("/out/1");

  // What DeviceInterface::sendMessage does with the value the explorer edited.
  param.push_value(0.25f);

  const auto tree = Device::ToDeviceExplorer(d.dev.get_root_node());
  const auto& settings = child(child(tree, "out"), "1").get<Device::AddressSettings>();

  REQUIRE(settings.value.get_type() == ossia::val_type::FLOAT);
  CHECK(*settings.value.target<float>() == 0.25f);
}

// The parameter kinds that have no user-editable value must stay out: this is
// what the MESSAGE-only filter was originally introduced for.
TEST_CASE("Valueless parameters are not editable", "[device][explorer]")
{
  test_device d;
  auto& node = ossia::net::find_or_create_node(d.dev.get_root_node(), "/midi");
  node.set_parameter(std::make_unique<valueless_parameter>(node));

  const auto tree = Device::ToDeviceExplorer(d.dev.get_root_node());
  const auto& midi = child(tree, "midi");

  CHECK_FALSE(midi.isEditable());
  CHECK_FALSE(midi.get<Device::AddressSettings>().ioType.has_value());
}

// The rule the tree conversion follows, stated once rather than as a type
// allow-list buried in the conversion (issue #2204 came from that allow-list).
TEST_CASE("only parameters with a readable value are mirrored", "[device][explorer]")
{
  CHECK(Device::hasEditableValue(ossia::parameter_type::MESSAGE));
  CHECK(Device::hasEditableValue(ossia::parameter_type::AUDIO));

  CHECK_FALSE(Device::hasEditableValue(ossia::parameter_type::MIDI));
  CHECK_FALSE(Device::hasEditableValue(ossia::parameter_type::TEXTURE));
  CHECK_FALSE(Device::hasEditableValue(ossia::parameter_type::GEOMETRY));
}
