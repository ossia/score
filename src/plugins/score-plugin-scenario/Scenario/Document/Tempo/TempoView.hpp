#pragma once
#include <Curve/CurveView.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>

#include <Scenario/Document/Tempo/TempoProcess.hpp>

namespace Scenario
{

class TempoView final : public Process::LayerView
{
public:
  explicit TempoView(QGraphicsItem* parent)
      : Process::LayerView{parent}
  {
    setZValue(1);
    setFlags(
        ItemClipsToShape | ItemClipsChildrenToShape | ItemIsSelectable
        | ItemIsFocusable);
    setAcceptDrops(true);
  }

  void setCurveView(Curve::View* view) { m_curveView = view; }
  ~TempoView() override { }

private:
  QPixmap pixmap() noexcept override
  {
    if (m_curveView)
      return m_curveView->pixmap();
    else
      return QPixmap();
  }

  void paint_impl(QPainter* painter) const override { }
  void dropEvent(QGraphicsSceneDragDropEvent* event) override
  {
    if (event->mimeData())
      dropReceived(event->pos(), *event->mimeData());
  }

  Curve::View* m_curveView{};
};

class TempoPresenter final
    : public Curve::CurveProcessPresenter<TempoProcess, TempoView>
{
public:
  explicit TempoPresenter(
      const Curve::Style& style,
      const Scenario::TempoProcess& layer,
      TempoView* view,
      const Process::Context& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    // only disable the curve when using the first 2 inlets
    for (int i = 0; i < 2; i++)
    {
      QObject::connect(
          layer.inlets()[i],
          &Process::Inlet::addressChanged,
          this,
          [this] { disableIfNeeded(); });

      QObject::connect(
          layer.inlets()[i],
          &Process::Inlet::cablesChanged,
          this,
          [this] { disableIfNeeded(); });
    }
  }

  void setFullView() override { m_curve.setBoundedMove(false); }

private:
  void disableIfNeeded()
  {
    bool should_disable{false};

    // only disable the curve when using the first 2 inlets
    for (int i = 0; i < 2; i++)
    {
      const auto inlet{m_process.inlets()[i]};

      if (inlet->address().isSet()
          || !inlet->cables().empty())
        should_disable = true;
    }

    if (should_disable)
      m_curve.disable();
    else
      m_curve.enable();
  }
};
}
