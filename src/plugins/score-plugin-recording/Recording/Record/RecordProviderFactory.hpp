#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Recording/Record/RecordTools.hpp>

#include <core/document/DocumentView.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/Interface.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/ObjectMatches.hpp>

#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <score_plugin_recording_export.h>

#include <verdigris>
#include <QApplication>

namespace Scenario
{
class ProcessModel;
}
namespace Explorer
{
class DeviceExplorerModel;
}
namespace Recording
{
struct RecordContext : public QObject
{
  W_OBJECT(RecordContext)
public:
  using clock = std::chrono::steady_clock;
  RecordContext(Scenario::ProcessModel&, Scenario::Point pt);
  RecordContext(const RecordContext& other) = delete;
  RecordContext(RecordContext&& other) = delete;
  RecordContext& operator=(const RecordContext& other) = delete;
  RecordContext& operator=(RecordContext&& other) = delete;

  void start()
  {
    firstValueTime = clock::now();
    startTimer();
  }

  bool started() const
  {
    return firstValueTime.time_since_epoch() != clock::duration::zero();
  }

  TimeVal time() const { return GetTimeDifference(firstValueTime); }

  double timeInDouble() const
  {
    return GetTimeDifferenceInDouble(firstValueTime);
  }

  const score::DocumentContext& context;
  Scenario::ProcessModel& scenario;
  Explorer::DeviceExplorerModel& explorer;
  RecordCommandDispatcher dispatcher;

  Scenario::Point point;
  clock::time_point firstValueTime{};
  QTimer timer;

public:
  void startTimer() W_SIGNAL(startTimer);

public:
  void on_startTimer() {
    context.document.view()->viewDelegate().getWidget()->setEnabled(false);
    timer.start();
  }
  W_SLOT(on_startTimer)
};

struct RecordProvider
{
  virtual ~RecordProvider();
  virtual bool setup(const Box&, const RecordListening&) = 0;
  virtual void stop() = 0;
};

// A recording session.
class Recorder
{
public:
  Recorder(RecordContext& ctx) //: m_context{ctx}
  {
    // Algorithm :
    // 1. Get all selected addresses.
    // 2. Generate the concrete recorders that match these addresses.
    // Use settings to select them recursively or not..
    // 3. Create a box corresponding to the case where we record
    // in the void, in an existing interval, starting from a state / event...

    // Have a "record as" attribute on nodes to indicate if
    // it should be recorded as a message, or automation, or ...
  }

  bool setup() { return false; }

  void stop() { }

private:
  // RecordContext& m_context;
  std::vector<std::unique_ptr<RecordProvider>> m_recorders;
};

// Sets-up the recording of a single class.
template <typename T>
class SingleRecorder final : public QObject
{
public:
  T recorder;

  SingleRecorder(RecordContext& ctx)
      : recorder{ctx}
  {
  }

  bool setup()
  {
    RecordContext& ctx = recorder.context;
    //// Device tree management ////
    // Get the listening of the selected addresses
    auto recordListening = GetAddressesToRecordRecursive(ctx.explorer);
    if (recordListening.empty())
      return false;

    // Disable listening for everything
    ctx.explorer.deviceModel().listening().stop();

    // Create the processes, etc.
    Box box = CreateBox(ctx);

    if (!recorder.setup(box, recordListening))
    {
      ctx.explorer.deviceModel().listening().restore();
      return false;
    }

    //// Start the record timer ////
    ctx.timer.stop();
    ctx.timer.setTimerType(Qt::PreciseTimer);
    ctx.timer.setInterval(ReasonableUpdateInterval(recorder.count()));
    QObject::connect(&ctx.timer, &QTimer::timeout, this, [&, box]() {
      // Move end event by the current duration.
      auto& cur_date = box.interval.date();
      box.moveCommand.update(
          ctx.scenario,
          {},
          box.endEvent,
          cur_date + GetTimeDifference(ctx.firstValueTime),
          0,
          true);

      box.moveCommand.redo(ctx.context);
    });

    // In case where the software is exited
    // during recording.
    QObject::connect(
        &ctx.scenario,
        &IdentifiedObjectAbstract::identified_object_destroyed,
        this,
        [&]() { ctx.timer.stop(); });

    return true;
  }

  void stop()
  {
    RecordContext& ctx = recorder.context;
    ctx.timer.stop();

    recorder.stop();

    ctx.explorer.deviceModel().listening().restore(); // Commit
    ctx.dispatcher.commit();
    ctx.context.document.view()->viewDelegate().getWidget()->setEnabled(true);
  }

private:
};

class SCORE_PLUGIN_RECORDING_EXPORT RecorderFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(RecorderFactory, "64999184-a705-4686-b967-14e8f79692f1")
public:
  virtual ~RecorderFactory();

  virtual Priority
  matches(const Device::Node&, const score::DocumentContext& ctx)
      = 0;

  virtual std::unique_ptr<RecordProvider>
  make(const Device::NodeList&, const score::DocumentContext& ctx) = 0;
};
}
