#pragma once
#include <QObject>

#include <score_lib_process_export.h>

#include <cstdint>
#include <memory>

#include <verdigris>

namespace ossia::telemetry
{
struct playhead_tap;
struct playhead_slot;
}

namespace Execution
{
/**
 * What the execution reports back to the interface, as execution components
 * see it: they sit below the engine that implements it.
 *
 * Everything read here is informative and follows the execution-update
 * setting: nothing comes back while it is off.
 */
class SCORE_LIB_PROCESS_EXPORT TelemetryInterface : public QObject
{
  W_OBJECT(TelemetryInterface)
public:
  struct Playhead
  {
    int index{-1};
    uint32_t generation{};
    explicit operator bool() const noexcept { return index >= 0; }
  };

  using QObject::QObject;
  ~TelemetryInterface() override;

  //! The execution thread writes where something is into `tap`; the latest
  //! value comes back at each update.
  virtual Playhead registerPlayhead(std::shared_ptr<ossia::telemetry::playhead_tap> tap)
      = 0;
  virtual void release(Playhead p) = 0;
  //! The latest value, or nullptr when there is none.
  virtual const ossia::telemetry::playhead_slot* playhead(Playhead p) const noexcept = 0;

  //! A new frame came back from the execution.
  void updated() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, updated)
};
}
