#pragma once
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Recording/Record/RecordData.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
#include <Recording/Record/RecordTools.hpp>

#include <score/tools/std/HashMap.hpp>

#include <verdigris>
namespace Curve
{
namespace Settings
{
class Model;
}
}
namespace Recording
{
struct RecordContext;
// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class AutomationRecorder : public QObject, public RecordProvider, public Nano::Observer
{
  W_OBJECT(AutomationRecorder)
public:
  RecordContext& context;
  AutomationRecorder(RecordContext& ctx);

  bool setup(const Box&, const RecordListening&) override;
  void stop() override;

  void commit();

  score::hash_map<State::Address, RecordData> numeric_records;
  score::hash_map<State::Address, std::array<RecordData, 2>> vec2_records;
  score::hash_map<State::Address, std::array<RecordData, 3>> vec3_records;
  score::hash_map<State::Address, std::array<RecordData, 4>> vec4_records;
  score::hash_map<State::Address, std::vector<RecordData>> list_records;

public:
  void firstMessageReceived() W_SIGNAL(firstMessageReceived);

private:
  void messageCallback(const State::Address& addr, const ossia::value& val);
  void parameterCallback(const State::Address& addr, const ossia::value& val);

  bool finish(State::AddressAccessor addr, const RecordData& dat, const TimeVal& msecs, bool, int);
  const Curve::Settings::Model& m_settings;
  Curve::Settings::Mode m_recordingMode{};
  std::vector<QPointer<Device::DeviceInterface>> m_recordCallbackConnections;

  // TODO see this :
  // http://stackoverflow.com/questions/34596768/stdunordered-mapfind-using-a-type-different-than-the-key-type
};
/*
class AutomationRecorderFactory final : public RecorderFactory
{
  Priority
  matches(const Device::Node&, const score::DocumentContext& ctx) override;

  std::unique_ptr<RecordProvider>
  make(const Device::NodeList&, const score::DocumentContext& ctx) override;
};
*/
}
