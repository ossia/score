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

    QGraphicsItem* makeDefaultItem(EffectModel& effect, const Effect::ProcessModel& object,
                                   const score::DocumentContext& doc)
    {
      auto fx_item = new Process::RectItem(this);
      EffectUi fx_ui{effect, fx_item, {}};


      {
        auto title = new QWidget;
        auto title_lay = new QHBoxLayout{title};
        auto gw = new QGraphicsProxyWidget{fx_item};
        gw->setWidget(title);
        fx_ui.title = gw;

        auto name = new QLabel{effect.prettyName(), title};

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
        }, Qt::QueuedConnection);

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

      inlet_cons.reserve(effect.inlets().size() + inlet_cons.size());
      for(auto& e : effect.inlets())
      {
        auto inlet = dynamic_cast<Process::ControlInlet*>(e);
        if(!inlet)
          continue;

        auto con = connect(inlet, &Process::ControlInlet::uiVisibleChanged,
                this, [this,&doc,fx=&effect,inlet] (bool vis) {
          if(vis)
          {
            for(auto& e : this->effects)
            {
              if(&e.effect == fx)
              {
                setupInlet(*inlet, e, doc);

                updateSize(e);
                break;
              }
            }
          }
          else
          {
            for(auto& e : this->effects)
            {
              if(&e.effect == fx)
              {
                disableInlet(*inlet, e, doc);

                updateSize(e);
                break;
              }
            }
          }
        });
        inlet_cons.push_back(con);
        if(inlet->uiVisible())
        {
          setupInlet(*inlet, fx_ui, doc);
        }
      }

      effects.push_back(fx_ui);
      updateSize(fx_ui);
      return fx_item;
    }

    void setup(const Effect::ProcessModel& object,
               const score::DocumentContext& doc)
    {
      auto items = childItems();
      effects.clear();
      for(const auto& con : inlet_cons)
        QObject::disconnect(con);
      inlet_cons.clear();
      for(auto item : items)
      {
        this->scene()->removeItem(item);
        delete item;
      }

      double pos_x = 0;
      for(EffectModel& effect : object.effects())
      {
        auto item = effect.makeItem(doc);
        if(item)
        {
          EffectUi fx_ui{effect, item, {}, {}};
          effects.push_back(fx_ui);
          item->setParentItem(this);
        }
        else
        {
          item = makeDefaultItem(effect, object, doc);
        }
        SCORE_ASSERT(item);
        item->setPos(pos_x, 0);
        pos_x += item->boundingRect().width();
      }
    }

    std::vector<QMetaObject::Connection> inlet_cons;

    struct ControlUi
    {
        Process::ControlInlet* inlet;
        Process::RectItem* rect;
    };
    struct EffectUi
    {
        const EffectModel& effect;
        QGraphicsItem* fx_item{};
        QGraphicsProxyWidget* title{};
        std::vector<ControlUi> widgets;
    };
    void updateSize(const EffectUi& fx)
    {
      double w = 180;
      double h = 30 + fx.title->rect().height();
      for(auto& widg : fx.widgets)
      {
        h += widg.rect->boundingRect().height();
      }
      safe_cast<Process::RectItem*>(fx.fx_item)->setRect({0, 0, w, h});
    }

    std::vector<EffectUi> effects;

    void disableInlet(
        Process::ControlInlet& inlet,
        EffectUi& fx_ui,
        const score::DocumentContext& doc)
    {
      for(auto it = fx_ui.widgets.begin(); it != fx_ui.widgets.end(); )
      {
        if(it->inlet == &inlet)
        {
          this->scene()->removeItem(it->rect);
          auto h = it->rect->boundingRect().height();
          delete it->rect;
          for(; it != fx_ui.widgets.end(); ++it)
          {
            auto pos = it->rect->pos();
            pos.ry() -= h;
            it->rect->setPos(pos);
          }
          break;
        }
        else
        {
          ++it;
        }
      }
    }

    void setupInlet(
        Process::ControlInlet& inlet,
        EffectUi& fx_ui,
        const score::DocumentContext& doc)
    {
      auto item = new Process::RectItem{fx_ui.fx_item};

      double pos_y =
      (fx_ui.widgets.empty())
          ? 40
          : fx_ui.widgets.back().rect->boundingRect().height() + fx_ui.widgets.back().rect->pos().y();


      auto port = Dataflow::setupInlet(inlet, doc, item, this);

      auto lab = new Scenario::SimpleTextItem{item};
      lab->setColor(ScenarioStyle::instance().EventDefault);
      lab->setText(inlet.customData());

      struct SliderInfo {
          static float getMin() { return 0.; }
          static float getMax() { return 1.; }
      };
      QWidget* widg = Process::FloatSlider::make_item(SliderInfo{}, inlet, doc, nullptr, this);
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

      port->setPos(7., h / 2.);

      item->setPos(0, pos_y);
      item->setRect(QRectF{0., 0, 170., h});
      fx_ui.widgets.push_back({&inlet, item});


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

      m_view->setup(static_cast<const Effect::ProcessModel&>(model), ctx);
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
