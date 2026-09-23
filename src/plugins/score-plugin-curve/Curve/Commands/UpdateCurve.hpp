#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <optional>
#include <vector>

namespace Curve
{
class Model;

//! A segment before and after an UpdateCurve: no `before` for a segment it
//! creates, no `after` for one it removes.
struct CurveChange
{
  Id<SegmentModel> id;
  std::optional<SegmentData> before;
  std::optional<SegmentData> after;
};

//! Replaces a curve by `segments`, keeping only what differs: the cost of
//! undo, redo and of the command on the stack is that of the change.
class SCORE_PLUGIN_CURVE_EXPORT UpdateCurve final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), UpdateCurve, "Update Curve")
public:
  UpdateCurve(const Model& model, const std::vector<SegmentData>& segments);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  //! For an ongoing edit: `model` is as the last redo() left it.
  void update(const Model& model, const std::vector<SegmentData>& segments);

  const std::vector<CurveChange>& changes() const noexcept { return m_changes; }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  //! False, changing nothing, if the result would not be a valid curve.
  bool setChanges(const Model& model, const std::vector<SegmentData>& segments);
  void apply(const score::DocumentContext& ctx, bool forward) const;

  Path<Model> m_model;
  std::vector<CurveChange> m_changes;
  //! Set by update() for the next redo(): what the previous redo() changed and
  //! the new changes leave alone, back to its state before the command.
  mutable std::vector<CurveChange> m_revert;
};
}
