#include "DefaultEffectItem.hpp"

namespace Media::Effect
{
DefaultEffectItem::DefaultEffectItem(
    const Process::ProcessModel& effect,
    const score::DocumentContext& doc,
    score::RectItem* root):
  score::RectItem{root}
{
  QObject::connect(
        &effect, &Process::ProcessModel::controlAdded,
        this, [&] (const Id<Process::Port>& id) {
    auto inlet = safe_cast<Process::ControlInlet*>(effect.inlet(id));
    setupInlet(*inlet, doc);
    this->setRect(this->childrenBoundingRect());
  });

  for(auto& e : effect.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if(!inlet)
      continue;

    setupInlet(*inlet,  doc);
  }

  QObject::connect(&effect, &Process::ProcessModel::inletsChanged,
                   this, [&] {
    const auto& c = this->childItems();
    for(auto child : c)
    {
      this->scene()->removeItem(child);
      delete child;
    }

    for(auto& e : effect.inlets())
    {
      auto inlet = qobject_cast<Process::ControlInlet*>(e);
      if(!inlet)
        continue;

      setupInlet(*inlet,  doc);
    }
    this->setRect(this->childrenBoundingRect());
  });

  // TODO same for outlets if we have control outlets one day
}

void DefaultEffectItem::setupInlet(Process::ControlInlet& inlet, const score::DocumentContext& doc)
{
  auto item = new score::EmptyRectItem{this};

  double pos_y = this->childrenBoundingRect().height();

  auto port = Dataflow::setupInlet(inlet, doc, item, this);

  auto lab = new Scenario::SimpleTextItem{item};
  lab->setColor(ScenarioStyle::instance().EventDefault);
  if(inlet.customData().isEmpty())
    lab->setText(tr("Control"));
  else
    lab->setText(inlet.customData());
  lab->setPos(15, 2);

  QGraphicsItem* widg{};
  auto& dom = inlet.domain().get();
  if(bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct {
      float min, max;
      float getMin() const { return min; }
      float getMax() const { return max; }
    } info{min, max};
    widg = Control::FloatSlider::make_item(info, inlet, doc, nullptr, this);
  }
  else
  {
    struct SliderInfo {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
    };
    widg = Control::FloatSlider::make_item(SliderInfo{}, inlet, doc, nullptr, this);
  }

  widg->setParentItem(item);
  widg->setPos(15, lab->boundingRect().height());

  auto h = std::max(20., (qreal)(widg->boundingRect().height() + lab->boundingRect().height() + 2.));

  port->setPos(7., h / 2.);

  item->setPos(0, pos_y);
  item->setRect(QRectF{0., 0, 170., h});
}

}
