#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Curve/Segment/CurveSegmentData.hpp>

#include <score_plugin_automation_export.h>

#include <array>
#include <optional>
#include <vector>

namespace Automation
{
/**
 * A value take of the value looper, as an automation can play it: one curve
 * of steps for each number of its values. The file format is described with
 * Nodes::Looper::value_export.
 */
struct SCORE_PLUGIN_AUTOMATION_EXPORT ValueTake
{
  static constexpr int max_components = 32;
  //! Numbers of each value: 1 for numbers, n for vectors of n, the most a
  //! list has for lists. A value that has fewer leaves the others where they
  //! were; before a number first comes, its first value holds.
  int components{};
  //! Some values are lists: the curves are named by their index.
  bool lists{};
  double seconds{};
  std::array<double, max_components> min{}, max{};
  struct step
  {
    //! From 0 to 1 over the take.
    double x;
    std::array<double, max_components> value;
  };
  std::vector<step> steps;

  //! Nothing when the file is not a value take an automation can play.
  static std::optional<ValueTake> parse(const QByteArray& json);

  //! The curve of `component`, its values from 0 to 1 between min and max:
  //! each value held to the next, the last one from the take's start up to
  //! the first when it does not start there.
  std::vector<Curve::SegmentData> segments(int component) const;
};

class ValueTakeDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("ed6975a6-407a-41a2-8e16-bb95932c9156")

public:
  QSet<QString> fileExtensions() const noexcept override;
  void dropData(
      std::vector<ProcessDrop>& drops, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override;
};
}
