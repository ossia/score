#include "EffectProcessLayer.hpp"

#include <Media/Commands/InsertEffect.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectPainting.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDrag>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QPainter>
#include <QUrl>

namespace score::mime
{
inline constexpr auto effect()
{
  return "application/x-score-fxdata";
}
}
namespace Media::Effect
{
static optional<int> m_lit{};
static qreal m_litHeight{};
class EffectItem final
    : public ::Effect::ItemBase
{
  const View& m_view;
  const Process::ProcessModel& m_model;
  const Process::LayerContext& m_context;
public:
  EffectItem(
      const View& view,
      const Process::ProcessModel& effect,
      const Process::LayerContext& ctx,
      const Process::LayerFactoryList& fact,
      QGraphicsItem* parent)
    : ::Effect::ItemBase{effect, ctx.context, parent}
    , m_view{view}
    , m_model{effect}
    , m_context{ctx}
  {
    // Main item
    if (auto factory = fact.findDefaultFactory(effect))
    {
      m_fx = factory->makeItem(m_model, m_context.context, this);
    }

    if (!m_fx)
    {
      m_fx = new DefaultEffectItem{m_model, m_context.context, this};
    }

    m_fx->setParentItem(this);
    m_fx->setPos({0, TitleHeight});

    /// Rects
    // TODO bind
    connect(m_fx, &score::ResizeableItem::sizeChanged,
            this, &EffectItem::updateSize);

    // In & out ports
    resetInlets();
    resetOutlets();
    con(effect, &Process::ProcessModel::inletsChanged,
        this, &EffectItem::resetInlets);
    con(effect,
        &Process::ProcessModel::outletsChanged,
        this, &EffectItem::resetOutlets);

    updateSize();
  }

private:
  void updateSize()
  {
    setSize(m_fx->boundingRect().size());
  }

  void setSize(QSizeF sz)
  {
    if(sz != m_contentSize)
    {
      prepareGeometryChange();
      m_contentSize = sz;
      if(m_ui)
      {
        m_ui->setParentItem(nullptr);
      }

      const auto r = boundingRect();

      for(auto& outlet : m_outlets)
      {
        outlet->setPos(outlet->pos().x(), r.height() + OutletY0);
      }
      m_view.recomputeItemPositions();

      if(m_ui)
      {
        m_ui->setParentItem(this);
        m_ui->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
      }
      update();
    }
  }

  void resetInlets()
  {
    qDeleteAll(m_inlets);
    m_inlets.clear();
    qreal x = InletX0;
    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    for (Process::Inlet* port : m_model.inlets())
    {
      if (port->hidden)
        continue;
      Process::PortFactory* fact = portFactory.get(port->concreteKey());
      auto item = fact->makeItem(*port, m_context.context, this, this);
      item->setPos(x, InletY0);
      m_inlets.push_back(item);

      x += PortSpacing;
    }

    m_label->setPos(QPointF{x, 0.});
    updateSize();
  }

  void resetOutlets()
  {
    qDeleteAll(m_outlets);
    m_outlets.clear();
    qreal x = OutletX0;
    const qreal h = boundingRect().height() + OutletY0;
    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    for (Process::Outlet* port : m_model.outlets())
    {
      if (port->hidden)
        continue;
      Process::PortFactory* fact = portFactory.get(port->concreteKey());
      auto item = fact->makeItem(*port, m_context.context, this, this);
      item->setPos(x, h);
      m_outlets.push_back(item);

      x += PortSpacing;
    }
    updateSize();
  }

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override
  {
    paintNode(painter, m_selected, m_hover, boundingRect());
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    m_context.context.focusDispatcher.focus(&m_context.presenter);
    score::SelectionDispatcher{m_context.context.selectionStack}.setAndCommit({&m_model});
    event->accept();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    auto min_dist
        = (event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton))
              .manhattanLength()
          >= QApplication::startDragDistance();
    if (min_dist)
    {
      auto drag = new QDrag{this};
      QMimeData* mime = new QMimeData;

      auto json = Scenario::copyProcess(m_model);
      json["Path"] = toJsonObject(score::IDocument::path(m_model));
      m_litHeight = this->boundingRect().height();
      mime->setData(score::mime::effect(), QJsonDocument{json}.toJson());
      drag->setMimeData(mime);

      drag->exec();

      drag->deleteLater();
    }

    event->accept();
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
  }

  score::ResizeableItem* m_fx{};
  std::vector<Dataflow::PortItem*> m_inlets;
  std::vector<Dataflow::PortItem*> m_outlets;
};

View::View(QGraphicsItem* parent) : Process::LayerView{parent} {}

void View::setup(const ProcessModel& object, const Process::LayerContext& ctx)
{
  auto& doc = ctx.context;
  auto& fact = doc.app.interfaces<Process::LayerFactoryList>();
  m_effects.clear();

  double pos_x = 10;
  for (auto& effect : object.effects())
  {
    auto fx = std::make_shared<EffectItem>(*this, effect, ctx, fact, this);
    fx->setPos(pos_x, 2);
    pos_x += 10 + fx->size().width();
    m_effects.push_back(std::move(fx));
  }
}

int View::findDropPosition(QPointF pos) const
{
  int idx = m_effects.size();
  int i = 0;
  for (const auto& item : m_effects)
  {
    if (pos.x() < item->pos().x() + item->size().width() * 0.5)
    {
      idx = i;
      break;
    }
    i++;
  }
  return idx;
}

void View::setInvalid(bool b)
{
  m_invalid = b;
  update();
}

const std::vector<std::shared_ptr<EffectItem> >& View::effects() const
{
  return m_effects;
}

void View::recomputeItemPositions() const
{
  double pos_x = 10;
  for (auto& fx : m_effects)
  {
    fx->setPos(pos_x, 2);
    pos_x += 10 + fx->size().width();
  }
}

void View::paint_impl(QPainter* p) const
{
  if (m_invalid)
  {
    p->fillRect(boundingRect(), Process::Style::instance().AudioPortBrush().brush);
  }

  if (m_lit)
  {
    int idx = *m_lit;
    double w = 2.5;
    for(int i = 0; i < idx; i++)
    {
      w += 10 + m_effects[i]->size().width();
    }

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Process::Style::instance().TransparentPen());
    p->setBrush(Process::Style::instance().StateDot());
    p->drawRoundedRect(
          QRectF(w + 1., 2., 3., m_litHeight), 2., 2.);
    p->setRenderHint(QPainter::Antialiasing, false);
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
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

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev) { ev->accept(); }

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}

void View::dragEnterEvent(QGraphicsSceneDragDropEvent* ev)
{
  dragMoveEvent(ev);
  for(auto& item : m_effects)
  {
    item->setZValue(-1);
  }
}

void View::dragMoveEvent(QGraphicsSceneDragDropEvent* ev)
{
  m_lit = findDropPosition(ev->pos());
  ev->accept();
  update();
}

void View::dragLeaveEvent(QGraphicsSceneDragDropEvent* ev)
{
  m_lit = {};
  ev->accept();
  update();

  for(auto& item : m_effects)
  {
    item->setZValue(1);
  }
}

void View::dropEvent(QGraphicsSceneDragDropEvent* ev)
{
  dropReceived(ev->pos(), *ev->mimeData());
  m_lit = {};
  for(auto& item : m_effects)
  {
    item->setZValue(1);
  }
}

Presenter::Presenter(const ProcessModel& model, View* view, const Process::Context& ctx, QObject* parent)
  : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
{
  putToFront();
  connect(view, &View::pressed, this, [&] {
    m_context.context.focusDispatcher.focus(this);
  });

  connect(
        m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);
  connect(
        m_view,
        &View::dropReceived,
        this,
        [=](const QPointF& pos, const QMimeData& m) {
    int idx = view->findDropPosition(pos);
    on_drop(m, idx);
  });

  auto& m = static_cast<const Effect::ProcessModel&>(model);
  con(m, &Effect::ProcessModel::effectsChanged, this, [&] {
    m_view->setup(
          static_cast<const Effect::ProcessModel&>(model), m_context);
  });
  con(m, &Effect::ProcessModel::badChainingChanged, this, [&](bool b) {
    m_view->setInvalid(b);
  });

  m_view->setup(static_cast<const Effect::ProcessModel&>(model), m_context);
}

void Presenter::setWidth(qreal width, qreal defaultWidth) { m_view->setWidth(width); }

void Presenter::setHeight(qreal val) { m_view->setHeight(val); }

void Presenter::putToFront() { m_view->setVisible(true); }

void Presenter::putBehind() { m_view->setVisible(false); }

void Presenter::on_zoomRatioChanged(ZoomRatio) {}

void Presenter::parentGeometryChanged() {}

const Process::ProcessModel& Presenter::model() const { return m_layer; }

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.id();
}

void Presenter::on_drop(const QMimeData& mime, int pos)
{
  const auto& ctx = context().context;
  if (mime.hasFormat(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();

    auto cmd = new InsertEffect(m_layer, p.key, p.customData, pos);
    CommandDispatcher<> d{ctx.commandStack};
    d.submit(cmd);
    return;
  }
  else if (mime.hasFormat(score::mime::effect()))
  {
    auto json
        = QJsonDocument::fromJson(mime.data(score::mime::effect())).object();
    const auto pid = ossia::get_pid();
    const bool same_doc
        = (pid == json["PID"].toInt())
        && (ctx.document.id().val() == json["Document"].toInt());

    if (same_doc)
    {
      const auto old_p
          = fromJsonObject<Path<Process::ProcessModel>>(json["Path"]);
      if (auto obj = old_p.try_find(ctx))
      {
        if (obj->parent() == &m_layer)
        {
          QTimer::singleShot(0, [this, &ctx, id = obj->id(), pos] {
            CommandDispatcher<>{ctx.commandStack}.submit(
                  new MoveEffect(m_layer, id, pos));
          });
          return;
        }
      }
      else
      {
        // todo
        return;
      }
    }
  }
  else if (mime.hasFormat(score::mime::layerdata()))
  {
    QJsonObject json
        = QJsonDocument::fromJson(mime.data(score::mime::layerdata()))
        .object();

    if (json.isEmpty())
      return;
    auto cmd = new LoadEffect(m_layer, json, pos);
    CommandDispatcher<> d{ctx.commandStack};
    d.submit(cmd);
  }
  else if (mime.hasUrls())
  {
    bool all_layers = ossia::all_of(mime.urls(), [](const QUrl& u) {
      return QFileInfo{u.toLocalFile()}.suffix() == "layer";
    });
    if (all_layers)
    {
      auto path = mime.urls().first().toLocalFile();
      if (QFile f{path}; f.open(QIODevice::ReadOnly))
      {
        auto json = QJsonDocument::fromJson(f.readAll()).object();
        if (json.isEmpty())
          return;

        auto cmd = new LoadEffect(m_layer, json, pos);
        CommandDispatcher<> d{ctx.commandStack};
        d.submit(cmd);
      }
    }
    else
    {
      const auto& handlers
          = ctx.app.interfaces<Process::ProcessDropHandlerList>();

      if (auto res = handlers.getDrop(mime, ctx); !res.empty())
      {
        MacroCommandDispatcher<Media::DropEffectMacro> cmd{ctx.commandStack};
        score::Dispatcher_T disp{cmd};
        for (const auto& proc : res)
        {
          auto& p = proc.creation;
          auto create = new InsertEffect(m_layer, p.key, p.customData, pos);
          cmd.submit(create);
          if (auto fx = m_layer.effects().find(create->processId());
              fx != m_layer.effects().list().end())
          {
            if (proc.setup)
            {
              proc.setup(**fx, disp);
            }
          }
        }

        cmd.commit();
      }
    }
  }
}

}
