#pragma once
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/model/Skin.hpp>

#include <ossia/network/value/format_value.hpp>

#include <QPainter>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
namespace Ui::ValueDisplay
{
struct Node
{
  halp_meta(name, "Value display")
  halp_meta(c_name, "Display")
  halp_meta(category, "Monitoring")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Visualize an input value")
  halp_meta(uuid, "3f4a41f2-fa39-420f-ab0f-0af6b8409edb")
  halp_flag(fully_custom_item);
  struct
  {
    struct : halp::val_port<"in", ossia::value>
    {
      enum widget
      {
        control
      };
    } port;
  } inputs;

  struct Layer : public Process::EffectLayerView
  {
  public:
    ossia::value m_value;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto inl = static_cast<Process::ControlInlet*>(process.inlets().front());

      auto fact = portFactory.get(inl->concreteKey());
      auto port = fact->makePortItem(*inl, doc, this, this);
      port->setPos(0, 5);

      connect(
          inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        m_value = v;
        update();
      });
    }

    void reset()
    {
      m_value = ossia::value{};
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      if(!m_value.valid())
        return;

      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      p->drawText(boundingRect(), QString::fromStdString(fmt::format("{}", m_value)));

      p->setRenderHint(QPainter::Antialiasing, false);
    }
  };
};
}
