#include "AudioChainLayer.hpp"

#include <Media/ChainItem.hpp>
#include <Media/Commands/InsertEffect.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>
namespace
{
static std::optional<int> m_lit{};
}
namespace Media
{
View::View(QGraphicsItem* parent) : Process::LayerView{parent} { }

void View::setup(const ChainProcess& object, const Process::LayerContext& ctx)
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

const std::vector<std::shared_ptr<EffectItem>>& View::effects() const
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
  else
  {
    p->fillRect(boundingRect(), score::Skin::instance().Background2.main.brush);
  }

  if (m_lit && *m_lit > 0)
  {
    int idx = std::min((int)m_effects.size(), *m_lit);
    double w = 2.5;
    for (int i = 0; i < idx; i++)
    {
      w += 10 + m_effects[i]->size().width();
    }

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Process::Style::instance().TransparentPen());
    p->setBrush(Process::Style::instance().StateDot());
    p->drawRoundedRect(QRectF(w + 1., 2., 3., EffectItem::litHeight), 2., 2.);
    p->setRenderHint(QPainter::Antialiasing, false);
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if (ev->button() == Qt::RightButton)
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    pressed(ev->pos());
  }
  ev->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

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
  for (auto& item : m_effects)
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

  for (auto& item : m_effects)
  {
    item->setZValue(1);
  }
}

void View::dropEvent(QGraphicsSceneDragDropEvent* ev)
{
  dropReceived(ev->pos(), *ev->mimeData());
  m_lit = {};
  for (auto& item : m_effects)
  {
    item->setZValue(1);
  }
}

Presenter::Presenter(
    const ChainProcess& model,
    View* view,
    const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{model, view, ctx, parent}, m_view{view}
{
  putToFront();
  connect(view, &View::pressed, this, [&] { m_context.context.focusDispatcher.focus(this); });

  connect(m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);
  connect(m_view, &View::dropReceived, this, [=](const QPointF& pos, const QMimeData& m) {
    int idx = view->findDropPosition(pos);
    on_drop(m, idx);
  });

  auto& m = static_cast<const AudioChain::ProcessModel&>(model);
  con(m, &AudioChain::ProcessModel::badChainingChanged, this, [&](bool b) {
    m_view->setInvalid(b);
  });

  m.effects().added.connect<&Presenter::on_effectAdded>(this);
  m.effects().removed.connect<&Presenter::on_effectRemoved>(this);
  m.effects().orderChanged.connect<&Presenter::on_orderChanged>(this);
  m_view->setup(static_cast<const AudioChain::ProcessModel&>(model), m_context);
}

void Presenter::on_effectAdded(const Process::ProcessModel& p)
{
  m_view->setup(static_cast<const AudioChain::ProcessModel&>(model()), m_context);
}

void Presenter::on_effectRemoved(const Process::ProcessModel& p)
{
  m_view->setup(static_cast<const AudioChain::ProcessModel&>(model()), m_context);
}

void Presenter::on_orderChanged()
{
  m_view->setup(static_cast<const AudioChain::ProcessModel&>(model()), m_context);
}

void Presenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view->setWidth(width);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setVisible(true);
}

void Presenter::putBehind()
{
  m_view->setVisible(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio) { }

void Presenter::parentGeometryChanged() { }

void Presenter::on_drop(const QMimeData& mime, int pos)
{
  const auto& ctx = context().context;
  const auto& model = static_cast<const ChainProcess&>(m_process);
  if (mime.hasFormat(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();

    auto cmd = new InsertEffect(model, p.key, p.customData, pos);
    CommandDispatcher<> d{ctx.commandStack};
    d.submit(cmd);
    return;
  }
  else if (mime.hasFormat(score::mime::effect()))
  {
    auto json = readJson(mime.data(score::mime::effect()));
    const auto pid = ossia::get_pid();
    const bool same_doc
        = (pid == json["PID"].GetInt()) && (ctx.document.id().val() == json["Document"].GetInt());

    if (same_doc)
    {
      const auto old_p = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();
      if (auto obj = old_p.try_find(ctx))
      {
        if (obj->parent() == &model)
        {
          QTimer::singleShot(0, [&model, &ctx, id = obj->id(), pos] {
            CommandDispatcher<>{ctx.commandStack}.submit(new MoveEffect(model, id, pos));
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
    auto json = readJson(mime.data(score::mime::layerdata()));

    if ((json.IsArray() && json.GetArray().Empty())
        || (json.IsObject() && json.MemberCount() == 0))
      return;
    auto cmd = new LoadEffect(model, json, pos);
    CommandDispatcher<> d{ctx.commandStack};
    d.submit(cmd);
  }
  else if (mime.hasUrls())
  {
    bool all_layers = ossia::all_of(
        mime.urls(), [](const QUrl& u) { return QFileInfo{u.toLocalFile()}.suffix() == "layer"; });
    if (all_layers)
    {
      auto path = mime.urls().first().toLocalFile();
      if (QFile f{path}; f.open(QIODevice::ReadOnly))
      {
        auto json = readJson(f.readAll());
        if (!json.IsObject() || json.MemberCount() == 0)
          return;

        auto cmd = new LoadEffect(model, json, pos);
        CommandDispatcher<> d{ctx.commandStack};
        d.submit(cmd);
      }
    }
    else
    {
      const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

      if (auto res = handlers.getDrop(mime, ctx); !res.empty())
      {
        MacroCommandDispatcher<Media::DropEffectMacro> cmd{ctx.commandStack};
        score::Dispatcher_T disp{cmd};
        for (const auto& proc : res)
        {
          auto& p = proc.creation;
          auto create = new InsertEffect(model, p.key, p.customData, pos);
          cmd.submit(create);
          if (auto fx = model.effects().find(create->processId());
              fx != model.effects().list().end())
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
