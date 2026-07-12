#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/ZoomHelper.hpp>

#include <Scenario/Sequence/SequenceModel.hpp>
#include <Scenario/Sequence/SequenceView.hpp>

#include <score/model/Identifier.hpp>

#include <QVector>

#include <score_plugin_scenario_export.h>
#include <verdigris>

namespace Scenario
{
class TemporalIntervalPresenter;
}
namespace Dataflow
{
class PortItem;
}

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
  // Destroy and recreate all child section presenters from the current model.
  void rebuildSections();
  // Reposition existing section presenters based on current zoom + interval dates.
  void updateSectionLayout();
  // One port item per slot row, bound to the sequence-level outlet of the
  // row's front process parameter: a single port for all the instances of
  // that process across sections.
  void updateRowPorts();

  const SequenceModel& m_model;
  SequenceView& m_view;
  ZoomRatio m_zoom{};

  // One TemporalIntervalPresenter per section interval, owned by this presenter.
  QVector<Scenario::TemporalIntervalPresenter*> m_sectionPresenters;
  QVector<Dataflow::PortItem*> m_rowPorts;
  QVector<QMetaObject::Connection> m_rackConns;
};

} // namespace Sequence
