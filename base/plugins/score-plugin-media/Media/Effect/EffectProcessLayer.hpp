#pragma once

#include <Effect/EffectLayer.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Media/Commands/InsertEffect.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <QFileInfo>
#include <ossia/detail/thread.hpp>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QApplication>
#include <QPainter>
#include <QWindow>
#include <core/document/Document.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/GraphicWidgets.hpp>
#include <score/widgets/RectItem.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

namespace Media::Effect
{

class View final : public Process::LayerView
{
  bool m_invalid{};

public:
  struct ControlUi
  {
    Process::ControlInlet* inlet;
    score::RectItem* rect;
  };

  struct EffectUi
  {
    EffectUi(const Process::ProcessModel& fx, score::RectItem* rt)
        : effect{fx}, root_item{rt}
    {
    }
    ~EffectUi()
    {
      for (auto& con : cons)
        QObject::disconnect(con);
    }
    const Process::ProcessModel& effect;
    score::RectItem* root_item{};
    QGraphicsItem* fx_item{};
    score::RectItem* title{};
    std::vector<Dataflow::PortItem*> inlets;
    std::vector<Dataflow::PortItem*> outlets;

    std::vector<QMetaObject::Connection> cons;
  };

  void setInvalid(bool b)
  {
    m_invalid = b;
    update();
  }
  explicit View(QGraphicsItem* parent)
    : Process::LayerView{parent}
  {
  }

  void resetInlets(
      Process::ProcessModel& effect,
      const Process::LayerContext& ctx,
      QGraphicsItem* root,
      EffectUi& ui)
  {
    qDeleteAll(ui.inlets);
    ui.inlets.clear();
    qreal x = 10;
    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    for (Process::Inlet* port : effect.inlets())
    {
      if (port->hidden)
        continue;
      Process::PortFactory* fact = portFactory.get(port->concreteKey());
      auto item = fact->makeItem(*port, ctx.context, root, this);
      item->setPos(x, 21.);
      ui.inlets.push_back(item);

      x += 10.;
    }
  }

  void resetOutlets(
      Process::ProcessModel& effect,
      const Process::LayerContext& ctx,
      QGraphicsItem* root,
      EffectUi& ui)
  {
    qDeleteAll(ui.outlets);
    ui.outlets.clear();
    qreal x = 10;
    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    for (Process::Outlet* port : effect.outlets())
    {
      if (port->hidden)
        continue;
      Process::PortFactory* fact = portFactory.get(port->concreteKey());
      auto item = fact->makeItem(*port, ctx.context, root, this);
      item->setPos(x, 32.);
      ui.outlets.push_back(item);

      x += 10.;
    }
  }

  score::RectItem* makeTitle(
      Process::ProcessModel& effect,
      const Effect::ProcessModel& object,
      const Process::LayerContext& ctx,
      EffectUi& ui)
  {
    auto& doc = ctx.context;
    auto& pixmaps = Process::Pixmaps::instance();
    auto root = new score::RectItem{};
    root->setRect({0, 0, 170, 40});

    resetInlets(effect, ctx, root, ui);
    resetOutlets(effect, ctx, root, ui);
    ui.cons.push_back(
        con(effect, &Process::ProcessModel::inletsChanged, this,
            [&, root] { resetInlets(effect, ctx, root, ui); }));
    ui.cons.push_back(
        con(effect, &Process::ProcessModel::outletsChanged, this,
            [&, root] { resetOutlets(effect, ctx, root, ui); }));

    if(auto ui_btn = Process::makeExternalUIButton(effect, ctx.context, this, root))
      ui_btn->setPos({5, 4});

    auto rm_btn = new score::QGraphicsPixmapButton{pixmaps.rm_process_on, pixmaps.rm_process_off, root};
    connect(
        rm_btn, &score::QGraphicsPixmapButton::clicked, this,
        [&]() {
          auto cmd = new RemoveEffect{object, effect};
          CommandDispatcher<> disp{doc.commandStack};
          disp.submitCommand(cmd);
        },
        Qt::QueuedConnection);

    rm_btn->setPos({20, 4});

    auto label = new Scenario::SimpleTextItem{root};
    label->setText(effect.prettyName());
    label->setFont(ScenarioStyle::instance().Bold10Pt);
    label->setPos({35, 4});

    connect(root, &score::RectItem::clicked, this, [&] {
      doc.focusDispatcher.focus(&ctx.presenter);
      score::SelectionDispatcher{doc.selectionStack}.setAndCommit({&effect});
    });

    return root;
  }

  void
  setup(const Effect::ProcessModel& object, const Process::LayerContext& ctx)
  {
    auto& doc = ctx.context;
    auto& fact = doc.app.interfaces<Process::LayerFactoryList>();
    auto items = childItems();
    effects.clear();
    for (auto item : items)
    {
      this->scene()->removeItem(item);
      delete item;
    }

    double pos_x = 10;
    for (auto& effect : object.effects())
    {
      auto root_item = new score::RectItem(this);
      std::shared_ptr<EffectUi> fx_ui_
          = std::make_shared<EffectUi>(effect, root_item);
      auto& fx_ui = *fx_ui_;

      // Title
      auto title = makeTitle(effect, object, ctx, fx_ui);
      fx_ui.title = title;
      fx_ui.title->setParentItem(root_item);

      // Main item
      if (auto factory = fact.findDefaultFactory(effect))
      {
        fx_ui.fx_item = factory->makeItem(effect, doc, root_item);
      }

      if (!fx_ui.fx_item)
      {
        fx_ui.fx_item = new DefaultEffectItem{effect, doc, root_item};
      }

      fx_ui.fx_item->setParentItem(root_item);
      fx_ui.fx_item->setPos({0, fx_ui.title->boundingRect().height()});
      fx_ui.root_item->setRect(fx_ui.root_item->childrenBoundingRect());
      effects.push_back(fx_ui_);

      fx_ui.root_item->setRect(
          {0., 0., 170.,
           fx_ui.root_item->childrenBoundingRect().height() + 10.});
      fx_ui.root_item->setPos(pos_x, 0);
      pos_x += 10 + fx_ui.root_item->boundingRect().width();

      connect(
          &effect.selection, &Selectable::changed, root_item, [=](bool ok) {
            root_item->setHighlight(ok);
            title->setHighlight(ok);
          });
    }
  }


  int findDropPosition(QPointF pos)
  {
    int idx = effects.size();
    int i = 0;
    for(const auto& item : effects)
    {
      if(pos.x() < item->root_item->pos().x() + item->root_item->boundingRect().width() * 0.5)
      {
        idx = i;
        break;
      }
      i++;
    }
    return idx;
  }

private:
  std::vector<std::shared_ptr<EffectUi>> effects;
  void paint_impl(QPainter* p) const override
  {
    if (m_invalid)
    {
      p->fillRect(boundingRect(),
                  ScenarioStyle::instance().AudioPortBrush);
    }

    if(m_lit)
    {
      int idx = *m_lit;

      p->setRenderHint(QPainter::Antialiasing, false);
      p->setPen(ScenarioStyle::instance().TransparentPen);
      p->setBrush(ScenarioStyle::instance().StateDot.getBrush());
      p->drawRoundedRect(QRectF(2.5 + idx * 180, 15, 5, boundingRect().height() - 30), 4, 4);
      p->setRenderHint(QPainter::Antialiasing, false);
    }
  }
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
  {
    if (ev && ev->button() == Qt::RightButton)
    {
      askContextMenu(ev->screenPos(), ev->scenePos());
    }
    else
    {
      pressed(ev->pos());
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
    askContextMenu(ev->screenPos(), ev->scenePos());
    ev->accept();
  }

  void dragEnterEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    dragMoveEvent(ev);
  }
  void dragMoveEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    m_lit = findDropPosition(ev->pos());
    ev->accept();
    update();
  }
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    m_lit = {};
    ev->accept();
    update();
  }
  void dropEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    dropReceived(ev->pos(), *ev->mimeData());
    m_lit = {};
  }

  optional<int> m_lit{};
};

class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Effect::ProcessModel& model,
      View* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
      : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
  {
    putToFront();
    connect(view, &View::pressed, this, [&] {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(m_view, &View::askContextMenu,
            this, &Presenter::contextMenuRequested);
    connect(m_view, &View::dropReceived,
            this, [=] (const QPointF& pos, const QMimeData& m) {
      int idx = view->findDropPosition(pos);
      on_drop(m, idx);
    });

    auto& m = static_cast<const Effect::ProcessModel&>(model);
    con(m, &Effect::ProcessModel::effectsChanged, this, [&] {
      m_view->setup(
          static_cast<const Effect::ProcessModel&>(model), m_context);
    });
    con(m, &Effect::ProcessModel::badChainingChanged, this,
        [&](bool b) { m_view->setInvalid(b); });

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

  void on_drop(const QMimeData& mime, int pos)
  {
    const auto& ctx = context().context;
    if(mime.hasFormat(score::mime::processdata()))
    {
      Mime<Process::ProcessData>::Deserializer des{mime};
      Process::ProcessData p = des.deserialize();

      auto cmd = new InsertEffect(m_layer, p.key, p.customData, pos);
      CommandDispatcher<> d{ctx.commandStack};
      d.submitCommand(cmd);
    }
    else
    {
      bool all_layers = ossia::all_of(
            mime.urls(),
            [] (const QUrl& u) { return QFileInfo{u.toLocalFile()}.suffix() == "layer"; });

      QJsonObject json;
      if(mime.hasFormat(score::mime::layerdata()))
      {
        json = QJsonDocument::fromJson(mime.data(score::mime::layerdata())).object();
      }
      else if(mime.hasUrls() && all_layers)
      {
        auto path = mime.urls().first().toLocalFile();
        if(QFile f{path}; f.open(QIODevice::ReadOnly))
        {
          json = QJsonDocument::fromJson(f.readAll()).object();
        }
      }

      auto cmd = new LoadEffect(m_layer, json, pos);
      CommandDispatcher<> d{ctx.commandStack};
      d.submitCommand(cmd);

      // TODO refactor in some way with DropLayerInInterval ?
      /*
      const auto pid = ossia::get_pid();
      const bool same_doc = (pid == json["PID"].toInt()) && (ctx.document.id().val() == json["Document"] .toInt());
      const bool small_view = json["View"].toString() == "Small";
      const int slot_index = json["SlotIndex"].toInt();

      const auto old_p = fromJsonObject<Path<Process::ProcessModel>>(json["Path"]);
      if(same_doc)
      {
        if(auto obj = old_p.try_find(ctx))
        if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj->parent()))
        {
          if(small_view && (qApp->keyboardModifiers() & Qt::ALT))
          {
            m.moveSlot(*itv, interval, slot_index);
          }
          else
          {
            m.moveProcess(*itv, interval, obj->id());
          }
        }
      }
      else
      {
        // Just create a new process
        m.loadProcessInSlot(interval, json);
      }
      */
    }
  }

private:
  const Effect::ProcessModel& m_layer;
  View* m_view{};
};
}
