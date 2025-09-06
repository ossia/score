#pragma once
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <ossia/network/value/format_value.hpp>

#include <boost/container/devector.hpp>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
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

    halp::spinbox_i32<"Log", halp::range{1, 100, 1}> log;
  } inputs;

  struct Layer : public Process::EffectLayerView
  {
  public:
    boost::container::devector<ossia::value> values;

    Process::ControlInlet* value_inlet;
    Process::ControlInlet* log_inlet;

    mutable QString txt_cache;

    int logging() const noexcept
    {
      return std::max(1, ossia::convert<int>(log_inlet->value()));
    }
    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons(Qt::NoButton);

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      value_inlet = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      log_inlet = static_cast<Process::ControlInlet*>(process.inlets()[1]);

      auto fact = portFactory.get(value_inlet->concreteKey());
      auto port = fact->makePortItem(*value_inlet, doc, this, this);
      port->setPos(0, 5);

      connect(
          value_inlet, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        if(values.size() >= logging())
          values.pop_back();
        values.push_front(v);
        update();
      });

      connect(
          log_inlet, &Process::ControlInlet::valueChanged, this,
          [this](const ossia::value& v) {
        const int N = std::max(1, ossia::convert<int>(v));
        if(values.size() >= N)
          values.resize(N);
        update();
      });
    }

    void reset()
    {
      values.clear();
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      if(values.empty())
        return;

      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      {
        txt_cache.clear();
        for(auto& line : this->values)
          txt_cache += QString::fromStdString(fmt::format("{}\n", line));
        p->drawText(boundingRect().adjusted(10, 0, 0, 0), txt_cache);
      }

      p->setRenderHint(QPainter::Antialiasing, false);
    }
  };

  struct Presenter : public Process::EffectLayerPresenter
  {
      using Process::EffectLayerPresenter::EffectLayerPresenter;
      void fillContextMenu(
          QMenu& menu, QPoint pos, QPointF scenepos,
          const Process::LayerContextMenuManager& ) override
      {
        auto cp = menu.addAction(tr("Copy value"));
        connect(cp, &QAction::triggered, this, [this] {
          auto& v = *static_cast<Layer*>(this->m_view);
          qApp->clipboard()->setText(v.txt_cache);
        });
      }
  };
};
}
