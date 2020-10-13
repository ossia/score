#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Commands.hpp>
#include <Nodal/Presenter.hpp>
#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>


#include <QTimer>

namespace Nodal
{

Presenter::Presenter(const Model& layer, View* view, const Process::Context& ctx, QObject* parent)
    : Process::LayerPresenter{layer, view, ctx, parent}, m_model{layer}, m_view{view}
{
  bind(layer.nodes, *this);

  connect(view, &View::dropReceived, this, [&](const QPointF& pos, const QMimeData& mime) {
    const auto& ctx = context().context;
    if (mime.hasFormat(score::mime::processdata()))
    {
      Mime<Process::ProcessData>::Deserializer des{mime};
      Process::ProcessData p = des.deserialize();

      auto cmd = new CreateNode(layer, pos, p.key, p.customData);
      CommandDispatcher<> d{ctx.commandStack};
      d.submit(cmd);
    }
    else
    {
      // TODO refactor with EffectProcessLayer
      const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

      if (auto res = handlers.getDrop(mime, ctx); !res.empty())
      {
        MacroCommandDispatcher<DropNodesMacro> cmd{ctx.commandStack};
        score::Dispatcher_T disp{cmd};
        for (const auto& proc : res)
        {
          auto& p = proc.creation;
          // TODO fudge pos a bit
          auto create = new CreateNode(layer, pos, p.key, p.customData);
          cmd.submit(create);
          if (auto fx = layer.nodes.find(create->nodeId()); fx != layer.nodes.end())
          {
            if (proc.setup)
            {
              proc.setup(*fx, disp);
            }
          }
        }

        cmd.commit();
      }
    }
  });

  auto parentObj = m_model.parent();
  while (parentObj && !qobject_cast<Scenario::IntervalModel*>(parentObj))
  {
    parentObj = parentObj->parent();
  }

  if (parentObj)
  {
    auto& dur = static_cast<Scenario::IntervalModel*>(parentObj)->duration;
    m_con = con(ctx.execTimer, &QTimer::timeout, this, [&] {
      {
        float p = ossia::clamp((float)dur.playPercentage(), 0.f, 1.f);
        for (Process::NodeItem& node : m_nodes)
        {
          node.setPlayPercentage(p);
        }
      }
    });
  }

  connect(&m_model, &Model::resetExecution, this, [this] {
    for (Process::NodeItem& node : m_nodes)
    {
      node.setPlayPercentage(0.f);
    }
  });
}

Presenter::~Presenter()
{
  m_nodes.remove_all();
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
  m_defaultW = defaultWidth;
  const auto r = m_ratio * m_defaultW;
  for (Process::NodeItem& node : m_nodes)
  {
    node.setZoomRatio(r);
  }
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setOpacity(1);
}

void Presenter::putBehind()
{
  m_view->setOpacity(0.2);
}

void Presenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  m_ratio = ratio;
  const auto r = m_ratio * m_defaultW;
  for (Process::NodeItem& node : m_nodes)
  {
    node.setZoomRatio(r);
  }
}

void Presenter::parentGeometryChanged() { }

void Presenter::on_created(Process::ProcessModel& n)
{
  auto item = new Process::NodeItem{n, m_context.context, m_view};
  item->setPos(n.position());
  const auto r = m_ratio * m_defaultW;
  item->setZoomRatio(r);
  m_nodes.insert(item);
}

void Presenter::on_removing(const Process::ProcessModel& n)
{
  m_nodes.erase(n.id());
}
}
