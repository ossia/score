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
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <score/widgets/GraphicWidgets.hpp>
#include <Media/Commands/InsertEffect.hpp>

namespace Media
{
namespace Effect
{
using ProcessFactory = Process::GenericProcessModelFactory<Effect::ProcessModel>;

class View final : public Control::ILayerView
{
  public:
    struct ControlUi
    {
        Process::ControlInlet* inlet;
        Control::RectItem* rect;
    };

    struct EffectUi
    {
        const EffectModel& effect;
        Control::RectItem* root_item{};
        QGraphicsItem* fx_item{};
        QGraphicsItem* title{};
        std::vector<ControlUi> widgets;
    };

    explicit View(QGraphicsItem* parent)
      : Control::ILayerView{parent}
    {
    }

    QGraphicsItem* makeTitle(EffectModel& effect, const Effect::ProcessModel& object, const score::DocumentContext& doc)
    {
      auto root = new Control::RectItem{};
      root->setRect({0, 0, 170, 20});
      static const auto undock_off = QPixmap::fromImage(QImage(":/icons/undock_off.png").scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto undock_on  = QPixmap::fromImage(QImage(":/icons/undock_on.png") .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto close_off  = QPixmap::fromImage(QImage(":/icons/close_off.png") .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto close_on   = QPixmap::fromImage(QImage(":/icons/close_on.png")  .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

      auto ui_btn = new score::QGraphicsPixmapToggle{undock_on, undock_off, root};
      connect(ui_btn, &score::QGraphicsPixmapToggle::toggled,
              this, [=,&effect] (bool b) {
        if(b)
          effect.showUI();
        else
          effect.hideUI();
      });
      ui_btn->setPos({5, 4});

      auto rm_btn = new score::QGraphicsPixmapButton{close_on, close_off, root};
      connect(rm_btn, &score::QGraphicsPixmapButton::clicked,
              this, [&] () {
        auto cmd = new Commands::RemoveEffect{object, effect};
        CommandDispatcher<> disp{doc.commandStack}; disp.submitCommand(cmd);
      }, Qt::QueuedConnection);

      rm_btn->setPos({20, 4});

      auto label = new Scenario::SimpleTextItem{root};
      label->setText(effect.prettyName());

      label->setPos({35, 4});

      return root;
    }

    Control::EffectItem* makeDefaultItem(
        EffectModel& effect, const Effect::ProcessModel& object,
        const score::DocumentContext& doc,
        EffectUi& fx_ui)
    {
      auto item = new Control::EffectItem;
      fx_ui.fx_item = item;
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
      return item;

    }

    void setup(const Effect::ProcessModel& object,
               const score::DocumentContext& doc)
    {
      auto& fact = doc.app.interfaces<Media::Effect::EffectUIFactoryList>();
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
        auto root_item = new Control::RectItem(this);
        EffectUi fx_ui{effect, root_item, {}, {}, {}};

        // Title
        fx_ui.title = makeTitle(effect, object, doc);
        fx_ui.title->setParentItem(root_item);

        // Main item
        if(auto factory = fact.findDefaultFactory(effect))
        {
          fx_ui.fx_item = factory->makeItem(effect, doc, root_item);
        }

        if(!fx_ui.fx_item)
        {
          fx_ui.fx_item = makeDefaultItem(effect, object, doc, fx_ui);
        }
        SCORE_ASSERT(fx_ui.fx_item);

        fx_ui.fx_item->setParentItem(root_item);
        fx_ui.fx_item->setPos({0, fx_ui.title->boundingRect().height()});
        updateSize(fx_ui);
        effects.push_back(fx_ui);

        fx_ui.root_item->setRect({0., 0., 170., fx_ui.root_item->childrenBoundingRect().height() + 10.});
        fx_ui.root_item->setPos(pos_x, 0);
        pos_x += 5 + fx_ui.root_item->boundingRect().width();
      }
    }

    void updateSize(const EffectUi& fx)
    {
      safe_cast<Control::RectItem*>(fx.root_item)->setRect(fx.root_item->childrenBoundingRect());
    }


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
      auto item = new Control::RectItem{fx_ui.fx_item};

      double pos_y =
      (fx_ui.widgets.empty())
          ? 0
          : fx_ui.widgets.back().rect->boundingRect().height() + fx_ui.widgets.back().rect->pos().y();


      auto port = Dataflow::setupInlet(inlet, doc, item, this);

      auto lab = new Scenario::SimpleTextItem{item};
      lab->setColor(ScenarioStyle::instance().EventDefault);
      lab->setText(inlet.customData());
      lab->setPos(15, 2);

      struct SliderInfo {
          static float getMin() { return 0.; }
          static float getMax() { return 1.; }
      };

      QGraphicsItem* widg = Control::FloatSlider::make_item(SliderInfo{}, inlet, doc, nullptr, this);
      widg->setParentItem(item);
      widg->setPos(15, lab->boundingRect().height());

      auto h = std::max(20., (qreal)(widg->boundingRect().height() + lab->boundingRect().height() + 2.));

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

    std::vector<QMetaObject::Connection> inlet_cons;
    std::vector<EffectUi> effects;
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
