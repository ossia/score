#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/ZoomHelper.hpp>

#include <Scenario/Sequence/SequenceModel.hpp>
#include <Scenario/Sequence/SequenceView.hpp>

#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>
#include <verdigris>

namespace Sequence
{

class SCORE_PLUGIN_SCENARIO_EXPORT SequencePresenter final
    : public Process::LayerPresenter
{
  W_OBJECT(SequencePresenter)

public:
  SequencePresenter(
      const SequenceModel& model, SequenceView* view, const Process::Context& ctx,
      QObject* parent);
  ~SequencePresenter() override;

  static constexpr bool recommendedHeight = false;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;
  void putToFront() override;
  void putBehind() override;
  void on_zoomRatioChanged(ZoomRatio ratio) override;
  void parentGeometryChanged() override;

private:
  void updateHandles();

  const SequenceModel& m_model;
  SequenceView& m_view;
  ZoomRatio m_zoom{};
};

} // namespace Sequence
