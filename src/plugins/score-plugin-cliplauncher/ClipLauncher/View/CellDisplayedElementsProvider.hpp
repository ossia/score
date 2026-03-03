#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

namespace ClipLauncher
{

class CellDisplayedElementsProvider final
    : public Scenario::DisplayedElementsProvider
{
  SCORE_CONCRETE("c3d4e5f6-7a8b-9c0d-1e2f-3a4b5c6d7e8f")
public:
  bool matches(const Scenario::IntervalModel& cst) const override;
  Scenario::DisplayedElementsContainer
  make(Scenario::IntervalModel& cst) const override;
  Scenario::DisplayedElementsPresenterContainer make_presenters(
      ZoomRatio zoom, const Scenario::IntervalModel& m,
      const Process::Context& ctx, QGraphicsItem* view_parent,
      QObject* parent) const override;
};

} // namespace ClipLauncher
