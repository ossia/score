#pragma once
#include "CurveCommandObjectBase.hpp"

#include <score/model/Identifier.hpp>

#include <ossia/detail/hash_map.hpp>

namespace Curve
{
class SegmentModel;
//! Drags the segment clicked, or all the selected ones when it is one of
//! them, in time and value. A segment linked to a moved one and not moved
//! itself stretches to follow; a moved point stops at the others and at the
//! bounds of the curve.
class SCORE_PLUGIN_CURVE_EXPORT MoveSegmentCommandObject final
    : public CommandObjectBase
{
public:
  MoveSegmentCommandObject(
      const Model& model, Presenter* presenter, const score::CommandStackFacade& stack);
  ~MoveSegmentCommandObject();

  void on_press() override;

  void move();

  void release();

  void cancel();

private:
  bool m_pressed{};
  bool m_moved{};
  ossia::hash_set<int32_t> m_movedIds;
  // How far the moved points can go
  double m_dxMin{}, m_dxMax{}, m_dyMin{}, m_dyMax{};
};
}
