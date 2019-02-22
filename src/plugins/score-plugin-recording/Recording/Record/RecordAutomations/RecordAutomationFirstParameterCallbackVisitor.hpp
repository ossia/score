#pragma once
#include <Recording/Record/RecordManager.hpp>

namespace Recording
{
struct RecordAutomationFirstCallbackVisitor
{
  AutomationRecorder& recorder;
  const State::Address& addr;

  void operator()(std::array<float, 2> val);
  void operator()(std::array<float, 3> val);
  void operator()(std::array<float, 4> val);

  void handle_numeric(float newval);

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
