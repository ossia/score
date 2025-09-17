#pragma once

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
namespace Scenario
{
class DropOnCable : public QObject
{
  // Only things that make sense to drop: processes and presets (maybe layers?)
  // Find in which interval we wish to drop.
  // If both ends are in the same hierarchy level we drop the process in the interval

public:
  const ScenarioDocumentModel& sm;
  const Process::Context& m_context;
  const Dataflow::CableItem& item;
  const Process::Cable& cable = item.model();
  const Process::Port& source = cable.source().find(m_context);
  const Process::Port& sink = cable.sink().find(m_context);

  Scenario::IntervalModel* m_interval{};

  DropOnCable(
      const Dataflow::CableItem& item, const ScenarioDocumentModel& sm,
      const Process::Context& m_context)
      : sm{sm}
      , m_context{m_context}
      , item{item}
  {
  }

  void createPreset(QPointF pos, const QByteArray& presetData)
  {
    auto& procs = m_context.app.interfaces<Process::ProcessFactoryList>();
    if(auto preset = Process::Preset::fromJson(procs, presetData))
    {
      Scenario::loadPresetInCable(m_context, sm, *preset, cable);
    }
  }

  void createProcess(QPointF pos, const Process::ProcessDropHandler::ProcessDrop& proc)
  {
    Scenario::createProcessInCable(
        m_context, sm, proc.creation, proc.duration, proc.setup, cable);
  }

  void drop(const QPointF& pos, const QMimeData& mime)
  {
    // FIXME put it closer to the source or sink depending on drop position
    auto source_itv = Scenario::closestParentInterval(&source);
    //auto sink_itv = Scenario::closestParentInterval(&sink);
    if(!source_itv)
      return;

    m_interval = source_itv;

    // Something tricky here is that we have to separate mime data processing and command execution:
    // mime data processing has to happen synchronously as it gets deleted after the CableItem::dropEvent function returns.
    // On the other hand, we cannot run the command synchronously as it may delete the cable, which causes
    // the cableitem to be deleted too and thus it cannot finish the dropEvent without crashing.
    const auto& handlers = m_context.app.interfaces<Process::ProcessDropHandlerList>();

    if(mime.hasFormat(score::mime::layerdata()))
    {
      // TODO
    }
    else if(mime.hasFormat(score::mime::processpreset()))
    {
      const auto presetData = mime.data(score::mime::processpreset());

      QMetaObject::invokeMethod(this, [pos, presetData, this]() {
        createPreset(pos, presetData);
      }, Qt::QueuedConnection);
    }
    else if(auto res = handlers.getDrop(mime, m_context); !res.empty())
    {
      if(res.size() >= 1)
      {
        QMetaObject::invokeMethod(this, [pos, proc = res.front(), this]() {
          createProcess(pos, proc);
        }, Qt::QueuedConnection);
      }
    }
    else if(mime.hasUrls())
    {
      // TODO
    }
  }
};

class DropOnNode : public QObject
{
public:
  const ScenarioDocumentModel& sm;
  const Process::Context& m_context;
  const Process::NodeItem& item;

  Scenario::IntervalModel* m_interval{};

  DropOnNode(
      const Process::NodeItem& item, const ScenarioDocumentModel& sm,
      const Process::Context& m_context)
      : sm{sm}
      , m_context{m_context}
      , item{item}
  {
  }

  void createPreset(const QByteArray& presetData)
  {
    auto& old = item.model();
    auto& procs = m_context.app.interfaces<Process::ProcessFactoryList>();
    if(auto preset = Process::Preset::fromJson(procs, presetData))
    {
      if(preset->key.key == old.concreteKey())
      {
        auto& load_preset_ifaces
            = m_context.app.interfaces<Process::LoadPresetCommandFactoryList>();

        auto cmd = load_preset_ifaces.make(
            &Process::LoadPresetCommandFactory::make, old, *preset, m_context);
        CommandDispatcher<> disp{m_context.commandStack};
        disp.submit(cmd);
      }
      else
      {
        Scenario::Command::Macro m{
            new Scenario::Command::DropProcessInIntervalMacro, m_context};
        score::Dispatcher_T<Scenario::Command::Macro> disp{m};
        if(auto p = m.loadProcessFromPreset(*m_interval, *preset, old.position()))
        {
          linkNewProcess(p, m);
          m.removeProcess(*m_interval, old.id());
          m.commit();
        }
      }
    }
  }

  void createProcess(const Process::ProcessDropHandler::ProcessDrop& proc)
  {
    auto& old = item.model();
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, m_context};
    score::Dispatcher_T<Scenario::Command::Macro> disp{m};
    if(auto p = m.createProcessInNewSlot(*m_interval, proc.creation, old.position()))
    {
      if(proc.setup)
        proc.setup(*p, disp);

      // Give the same connections that the previous process had
      linkNewProcess(p, m);

      // Remove the previous process
      m.removeProcess(*m_interval, old.id());

      m.commit();
    }
  }

  void linkNewProcess(Process::ProcessModel* p, Scenario::Command::Macro& m)
  {
    auto& old = item.model();
    if(p->inlets().size() > 0)
    {
      const auto dst = p->inlets()[0];
      const auto type = dst->type();

      if(old.inlets().size() > 0)
      {
        auto& old_dst = old.inlets()[0];
        if(old_dst->type() == type)
        {
          for(auto& edge : old.inlets()[0]->cables())
          {
            auto& cable = edge.find(m_context);
            auto& src = cable.source().find(m_context);
            if(src.type() == type)
            {
              m.createCable(sm, src, *dst);
            }
          }
          m.setProperty<Process::Port::p_address>(*dst, old_dst->address());
        }
      }
    }

    if(p->outlets().size() > 0)
    {
      const auto src = p->outlets()[0];
      const auto type = src->type();

      if(old.outlets().size() > 0)
      {
        auto& old_src = old.outlets()[0];
        if(old_src->type() == type)
        {
          for(auto& edge : old.outlets()[0]->cables())
          {
            auto& cable = edge.find(m_context);
            auto& dst = cable.sink().find(m_context);
            if(dst.type() == type)
            {
              m.createCable(sm, *src, dst);
            }
          }
          m.setProperty<Process::Port::p_address>(*src, old_src->address());
          if(type == Process::PortType::Audio)
          {
            auto old_audio_src = safe_cast<Process::AudioOutlet*>(old_src);
            auto audio_src = safe_cast<Process::AudioOutlet*>(src);
            m.setProperty<Process::AudioOutlet::p_propagate>(
                *audio_src, old_audio_src->propagate());
          }

          src->setAddress(old_src->address());
        }
      }
    }
  }

  void drop(const QMimeData& mime)
  {
    // FIXME drop in nodal vs drop in scenario
    auto& model = item.model();
    m_interval = qobject_cast<Scenario::IntervalModel*>(model.parent());
    if(!m_interval)
      return;

    // Something tricky here is that we have to separate mime data processing and command execution:
    // mime data processing has to happen synchronously as it gets deleted after the NodeItem::dropEvent function returns.
    // On the other hand, we cannot run the command synchronously as it may delete the cable, which causes
    // the cableitem to be deleted too and thus it cannot finish the dropEvent without crashing.
    const auto& handlers = m_context.app.interfaces<Process::ProcessDropHandlerList>();

    if(mime.hasFormat(score::mime::layerdata()))
    {
      // TODO
    }
    else if(mime.hasFormat(score::mime::processpreset()))
    {
      const auto presetData = mime.data(score::mime::processpreset());
      QMetaObject::invokeMethod(this, [presetData, this]() {
        createPreset(presetData);
      }, Qt::QueuedConnection);
    }
    else if(auto res = handlers.getDrop(mime, m_context); !res.empty())
    {
      if(res.size() >= 1)
      {
        QMetaObject::invokeMethod(this, [proc = res.front(), this]() {
          createProcess(proc);
        }, Qt::QueuedConnection);
      }
    }
    else if(mime.hasUrls())
    {
      // TODO
    }
  }
};
}
