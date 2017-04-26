#pragma once
#include <Recording/Record/RecordManager.hpp>

namespace Recording
{

struct RecordAutomationCreationVisitor
{
  Device::Node& node;
  const Box& box;
  Device::AddressSettings& addr;
  std::vector<std::vector<State::Address>>& addresses;
  AutomationRecorder& recorder;

  RecordData makeCurve(float start_y);

  void handle_numeric(float val);

  void operator()(std::array<float, 2> val);
  void operator()(std::array<float, 3> val);
  void operator()(std::array<float, 4> val);

  void operator()(float f);
  void operator()(int f);
  void operator()(char f);
  void operator()(bool f);

  template <typename... T>
  void operator()(const T&...)
  {
  }
};
}
