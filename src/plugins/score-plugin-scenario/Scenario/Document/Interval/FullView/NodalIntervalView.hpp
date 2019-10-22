#pragma once
#include <score/graphics/RectItem.hpp>
#include <nano_observer.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Process/Dataflow/NodeItem.hpp>


namespace Scenario {

class NodalIntervalView
    : public score::EmptyRectItem
    , public Nano::Observer
{
public:
  NodalIntervalView(IntervalPresenter& pres, const Process::Context& ctx, QGraphicsItem* parent)
    : score::EmptyRectItem{parent}
    , m_presenter{pres}
    , m_context{ctx}
  {
    //setFlag(ItemHasNoContents, false);
    //setRect(QRectF{0, 0, 1000, 1000});
    auto& itv = pres.model();
    const auto r = m_presenter.zoomRatio() * m_presenter.model().duration.defaultDuration().toPixels(m_presenter.zoomRatio());
    for(auto& proc : itv.processes)
    {
      auto item = new Process::NodeItem{proc, m_context, this};
      m_nodeItems.push_back(item);
      item->setZoomRatio(r);
    }
    itv.processes.added.connect<&NodalIntervalView::on_processAdded>(*this);
    itv.processes.removing.connect<&NodalIntervalView::on_processRemoving>(*this);
  }

  void on_playPercentageChanged(double t)
  {
    for(Process::NodeItem* node : m_nodeItems)
    {
      node->setPlayPercentage(t);
    }
  }
  void on_processAdded(const Process::ProcessModel& proc)
  {
    auto item = new Process::NodeItem{proc, m_context, this};
    const auto r = m_presenter.zoomRatio() * m_presenter.model().duration.defaultDuration().toPixels(m_presenter.zoomRatio());

    m_nodeItems.push_back(item);
    item->setZoomRatio(r);
  }

  void on_processRemoving(const Process::ProcessModel& model)
  {
    for(auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
    {
      if(&(*it)->model() == &model)
      {
        delete (*it);
        m_nodeItems.erase(it);
        return;
      }
    }
  }

  void on_zoomRatioChanged(ZoomRatio ratio)
  {
    const auto r = m_presenter.zoomRatio() * m_presenter.model().duration.defaultDuration().toPixels(ratio);
    for(Process::NodeItem* node : m_nodeItems)
    {
      node->setZoomRatio(r);
    }
  }
/*
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
  {
    painter->fillRect(m_rect, Qt::blue);
  }
*/
private: const IntervalPresenter& m_presenter;
  const Process::Context& m_context;
  std::vector<Process::NodeItem*> m_nodeItems;
};

}
