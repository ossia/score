#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Media/Effect/Effect/Widgets/EffectListWidget.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Widgets.hpp>
#include <Process/LayerView.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <Media/Commands/InsertEffect.hpp>

namespace Media
{
namespace Effect
{
using ProcessFactory = Process::GenericProcessModelFactory<Effect::ProcessModel>;

class View final : public Process::ILayerView
{
  public:
    explicit View(QGraphicsItem* parent)
      : Process::ILayerView{parent}
    {
    }

    void setup(const Effect::ProcessModel& object,
               const score::DocumentContext& doc)
    {
      auto items = childItems();
      for(auto item : items)
      {
        this->scene()->removeItem(item);
        delete item;
      }

      double pos_x = 0;
      for(EffectModel& effect : object.effects())
      {
        auto fx_item = new Process::RectItem(this);
        fx_item->setPos(pos_x, 0);

        {
          auto title = new QWidget;
          auto title_lay = new QHBoxLayout{title};
          auto gw = new QGraphicsProxyWidget{fx_item};
          gw->setWidget(title);

          auto name = new QLabel{effect.metadata().getLabel(), title};

          auto uibtn = new QPushButton{"UI", title};
          uibtn->setCheckable(true);
          connect(uibtn, &QPushButton::toggled,
                  this, [=,&effect] (bool b) {
            if(b)
              effect.showUI();
            else
              effect.hideUI();
          });


          auto rm_but = new QPushButton{"x"};
          rm_but->setStyleSheet(
                      "QPushButton { "
                      "border-style: solid;"
                      "border-width: 2px;"
                      "border-color: black;"
                      "border-radius: 15px;}");
          connect(rm_but, &QPushButton::clicked,
                  this, [&] () {
                auto cmd = new Commands::RemoveEffect{object, effect};
                CommandDispatcher<> disp{doc.commandStack}; disp.submitCommand(cmd);
          });

          title_lay->addWidget(name);
          title_lay->addWidget(uibtn);
          title_lay->addWidget(rm_but);
          title_lay->addStretch();

          title->setMaximumWidth(150);
          title->setContentsMargins(0, 0, 0, 0);
          title->setPalette(Process::transparentPalette());
          title->setAutoFillBackground(false);
          title->setStyleSheet(Process::transparentStylesheet());
        }

        double pos_y = 40;
        for(int i = 1; i < effect.inlets().size(); i++)
        {
          auto inlet = static_cast<Process::ControlInlet*>(effect.inlets()[i]);
          connect(inlet, &Process::ControlInlet::uiVisibleChanged,
                  this, [&] { setup(object, doc); });
          if(inlet->uiVisible())
          {
            auto item = new Process::RectItem{fx_item};
            item->setPos(0, pos_y);

            auto port = Dataflow::setupInlet(*inlet, doc, item, this);

            auto lab = new Scenario::SimpleTextItem{item};
            lab->setColor(ScenarioStyle::instance().EventDefault);
            lab->setText(inlet->customData());

            struct SliderInfo {
                static float getMin() { return 0.; }
                static float getMax() { return 1.; }
            };
            QWidget* widg = Process::FloatSlider::make_item(SliderInfo{}, *inlet, doc, nullptr, this);
            widg->setMaximumWidth(150);
            widg->setContentsMargins(0, 0, 0, 0);
            widg->setPalette(Process::transparentPalette());
            widg->setAutoFillBackground(false);
            widg->setStyleSheet(Process::transparentStylesheet());

            auto wrap = new QGraphicsProxyWidget{item};
            wrap->setWidget(widg);
            wrap->setContentsMargins(0, 0, 0, 0);

            lab->setPos(15, 2);
            wrap->setPos(15, lab->boundingRect().height());

            auto h = std::max(20., (qreal)(widg->height() + lab->boundingRect().height() + 2.));
            item->setRect(QRectF{0., 0., 170., h});
            port->setPos(7., h / 2.);

            pos_y += h;
          }
        }

        pos_x += 180;
      }
    }


  private:
    void paint_impl(QPainter*) const override
    {

    }
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
    {
      if(ev && ev->button() == Qt::RightButton)
      {
        emit askContextMenu(ev->screenPos(), ev->scenePos());
      }
      else
      {
        emit pressed();
      }
      ev->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override
    {
      emit askContextMenu(ev->screenPos(), ev->scenePos());
      ev->accept();
    }

};

class Presenter final : public Process::LayerPresenter
{
  public:
    explicit Presenter(
        const Process::ProcessModel& model,
        View* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent)
      : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
    {
      putToFront();
      connect(view, &View::pressed, this, [&] {
        m_context.context.focusDispatcher.focus(this);
      });

      connect(
            m_view, &View::askContextMenu, this,
            &Presenter::contextMenuRequested);

      connect(&static_cast<const Effect::ProcessModel&>(model), &Effect::ProcessModel::effectsChanged,
              this, [&] {
        m_view->setup(static_cast<const Effect::ProcessModel&>(model), ctx);
      });
    }

    void setWidth(qreal val) override
    {
      m_view->setWidth(val);
    }
    void setHeight(qreal val) override
    {
      m_view->setHeight(val);
    }

    void putToFront() override
    {
      m_view->setVisible(true);
    }

    void putBehind() override
    {
      m_view->setVisible(false);
    }

    void on_zoomRatioChanged(ZoomRatio) override
    {
    }

    void parentGeometryChanged() override
    {
    }

    const Process::ProcessModel& model() const override
    {
      return m_layer;
    }
    const Id<Process::ProcessModel>& modelId() const override
    {
      return m_layer.id();
    }

  private:
    const Process::ProcessModel& m_layer;
    View* m_view{};
};

using LayerFactory = Process::
    GenericLayerFactory<Effect::ProcessModel, Effect::Presenter, Effect::View, Process::GraphicsViewLayerPanelProxy>;
}
}
