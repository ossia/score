#pragma once

#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
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
#include <Effect/EffectFactory.hpp>
#include <score/selection/SelectionDispatcher.hpp>
namespace Media::Effect
{

class View final : public Process::LayerView
{
  public:
    struct ControlUi
    {
        Process::ControlInlet* inlet;
        score::RectItem* rect;
    };

    struct EffectUi
    {
        const Process::ProcessModel& effect;
        score::RectItem* root_item{};
        QGraphicsItem* fx_item{};
        score::RectItem* title{};
    };

    explicit View(QGraphicsItem* parent)
      : Process::LayerView{parent}
    {
    }

    score::RectItem* makeTitle(Process::ProcessModel& effect
                             , const Effect::ProcessModel& object
                             , const Process::LayerContext& ctx)
    {
      auto& doc = ctx.context;
      auto root = new score::RectItem{};
      root->setRect({0, 0, 170, 20});
      static const auto undock_off = QPixmap::fromImage(QImage(":/icons/undock_off.png").scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto undock_on  = QPixmap::fromImage(QImage(":/icons/undock_on.png") .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto close_off  = QPixmap::fromImage(QImage(":/icons/close_off.png") .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
      static const auto close_on   = QPixmap::fromImage(QImage(":/icons/close_on.png")  .scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

      auto ui_btn = new score::QGraphicsPixmapToggle{undock_on, undock_off, root};
      connect(ui_btn, &score::QGraphicsPixmapToggle::toggled,
              this, [=,&effect] (bool b) {
        if(b)
        {
          auto& facts = ctx.context.app.interfaces<Process::LayerFactoryList>();
          if(auto fact = facts.findDefaultFactory(effect))
          {
            auto win = fact->makeExternalUI(effect, ctx.context, nullptr);
            win->show();
            auto c0 = connect(win, &QWindow::visibilityChanged, ui_btn, [=] (auto vis) {
              if(vis == QWindow::Hidden)
              {
                ui_btn->toggle();
                win->deleteLater();
              }
            });

            connect(ui_btn, &score::QGraphicsPixmapToggle::toggled,
                    win, [=] (bool b) {
              QObject::disconnect(c0);
              win->close();
              delete win;
            });
          }

        }
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

      connect(root, &score::RectItem::clicked,
              this, [&] {
        doc.focusDispatcher.focus(&ctx.presenter);
        score::SelectionDispatcher{doc.selectionStack}.setAndCommit({&effect});
      });

      return root;
    }

    void setup(const Effect::ProcessModel& object,
               const Process::LayerContext& ctx)
    {
      auto& doc = ctx.context;
      auto& fact = doc.app.interfaces<Process::LayerFactoryList>();
      auto items = childItems();
      effects.clear();
      for(auto item : items)
      {
        this->scene()->removeItem(item);
        delete item;
      }

      double pos_x = 0;
      for(auto& effect : object.effects())
      {
        auto root_item = new score::RectItem(this);
        EffectUi fx_ui{effect, root_item, {}, {}};

        // Title
        auto title = makeTitle(effect, object, ctx);
        fx_ui.title = title;
        fx_ui.title->setParentItem(root_item);

        // Main item
        if(auto factory = fact.findDefaultFactory(effect))
        {
          fx_ui.fx_item = factory->makeItem(effect, doc, root_item);
        }

        if(!fx_ui.fx_item)
        {
          fx_ui.fx_item = new DefaultEffectItem{effect, doc, root_item};
        }

        fx_ui.fx_item->setParentItem(root_item);
        fx_ui.fx_item->setPos({0, fx_ui.title->boundingRect().height()});
        fx_ui.root_item->setRect(fx_ui.root_item->childrenBoundingRect());
        effects.push_back(fx_ui);

        fx_ui.root_item->setRect({0., 0., 170., fx_ui.root_item->childrenBoundingRect().height() + 10.});
        fx_ui.root_item->setPos(pos_x, 0);
        pos_x += 5 + fx_ui.root_item->boundingRect().width();

        connect(&effect.selection, &Selectable::changed, root_item, [=] (bool ok) {
          root_item->setHighlight(ok);
          title->setHighlight(ok);
        });

      }
    }


    void disableInlet(
        Process::ControlInlet& inlet,
        EffectUi& fx_ui,
        const score::DocumentContext& doc)
    {
      /*
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
      }*/
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
        emit pressed(ev->pos());
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
        m_view->setup(static_cast<const Effect::ProcessModel&>(model), m_context);
      });

      m_view->setup(static_cast<const Effect::ProcessModel&>(model), m_context);
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

}
