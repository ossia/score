#pragma once
#include <Effect/EffectModel.hpp>
#include <Engine/Node/Layer.hpp>

namespace Media::Effect
{

class DefaultEffectItem:
    public Control::EffectItem
{
    Control::RectItem* rootItem{};
  public:
    DefaultEffectItem(
        Process::EffectModel& effect, const score::DocumentContext& doc, Control::RectItem* root):
      Control::EffectItem{root}
    {
      QObject::connect(
            &effect, &Process::EffectModel::controlAdded,
            this, [&] (const Id<Process::Port>& id) {
        auto inlet = safe_cast<Process::ControlInlet*>(effect.inlet(id));
        setupInlet(*inlet, doc);
        rootItem->setRect(rootItem->childrenBoundingRect());
      });

      for(auto& e : effect.inlets())
      {
        auto inlet = qobject_cast<Process::ControlInlet*>(e);
        if(!inlet)
          continue;

        setupInlet(*inlet,  doc);
      }
    }

    void setupInlet(
        Process::ControlInlet& inlet,
        const score::DocumentContext& doc)
    {
      auto item = new Control::RectItem{this};

      double pos_y = this->childrenBoundingRect().height();

      auto port = Dataflow::setupInlet(inlet, doc, item, this);

      auto lab = new Scenario::SimpleTextItem{item};
      lab->setColor(ScenarioStyle::instance().EventDefault);
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
};


}
