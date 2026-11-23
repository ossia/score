#include "RecordPortMenu.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Midi/MidiProcess.hpp>
#include <Process/Process.hpp>
#include <Recording/ApplicationPlugin.hpp>
#if SCORE_RECORDING_TEXTURES
#include <Recording/Video/TextureRecorder.hpp>
#endif

#include <score/application/GUIApplicationContext.hpp>

#include <QMenu>

namespace Recording
{
void RecordPortMenu::setupMenu(
    QMenu& menu, const Process::Port& port, const Process::Context& ctx)
{
  const auto type = port.type();
  bool texture = false;
#if SCORE_RECORDING_TEXTURES
  texture = type == Process::PortType::Texture && TextureRecorder::canRecord(port);
#endif
  if(type != Process::PortType::Audio && type != Process::PortType::Midi
     && type != Process::PortType::Message && !texture)
    return;
  auto* plug = ctx.app.findGuiApplicationPlugin<Recording::ApplicationPlugin>();
  if(!plug)
    return;

  if(!menu.actions().empty())
    menu.addSeparator();

  if(plug->recording(ctx))
  {
    auto stop = menu.addAction(QObject::tr("Stop recording"));
    QObject::connect(stop, &QAction::triggered, plug, [plug] { plug->stopSession(); });
    return;
  }

  const QPointer<const Process::Port> p = &port;
  const auto record = [plug, p, &ctx](RecordOptions opt) {
    if(p)
      plug->startSession(ctx, {p.data()}, opt);
  };

  auto sub = menu.addMenu(QObject::tr("Record"));
  {
    auto act = sub->addAction(QObject::tr("Record now"));
    QObject::connect(act, &QAction::triggered, plug, [=] { record(plug->defaultOptions()); });
  }
  // A texture is recorded as a video from when it starts
  if(texture)
  {
    auto act = sub->addAction(QObject::tr("Record when playback starts"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.start = RecordOptions::Start::OnPlay;
      record(opt);
    });
    auto file = sub->addAction(QObject::tr("Record only the file"));
    QObject::connect(file, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.target = RecordOptions::Target::FileOnly;
      record(opt);
    });
    return;
  }
  {
    auto act = sub->addAction(QObject::tr("Record from the next bar"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.startRate = 1.;
      record(opt);
    });
  }
  {
    auto act = sub->addAction(QObject::tr("Record from the next bar, counting in"));
    act->setToolTip(QObject::tr("The beats are clicked on the main output until it starts"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.startRate = 1.;
      opt.countIn = true;
      record(opt);
    });
  }
  {
    auto act = sub->addAction(QObject::tr("Record one bar from the next bar"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.startRate = 1.;
      opt.lengthBars = 1.;
      record(opt);
    });
  }
  // Over and over, a bar at a time: the passes are kept as the user chooses
  {
    auto passes = sub->addMenu(QObject::tr("Record passes of one bar"));
    using P = RecordOptions::Passes;
    const std::pair<P, QString> kinds[]{
        {P::Alternatives, QObject::tr("As alternatives, the last one playing")},
        {P::SideBySide, QObject::tr("Side by side, where they were played")},
        {P::LastOnly, QObject::tr("Keeping only the last one")}};
    for(const auto& [kind, text] : kinds)
    {
      auto act = passes->addAction(text);
      QObject::connect(act, &QAction::triggered, plug, [=, kind = kind] {
        auto opt = plug->defaultOptions();
        opt.startRate = 1.;
        opt.lengthBars = 0.;
        opt.passes = kind;
        opt.passBars = 1.;
        record(opt);
      });
    }
  }
  if(type != Process::PortType::Audio)
  {
    auto act = sub->addAction(
        type == Process::PortType::Midi ? QObject::tr("Record from the first note")
                                        : QObject::tr("Record from the first value"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.start = RecordOptions::Start::FirstEvent;
      record(opt);
    });
  }
  {
    auto act = sub->addAction(QObject::tr("Record when playback starts"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.start = RecordOptions::Start::OnPlay;
      record(opt);
    });
  }
  // MIDI into the piano rolls of its box
  if(type == Process::PortType::Midi)
  {
    if(auto* box = Scenario::closestParentInterval(Process::parentProcess(&port)))
    {
      for(const auto& proc : box->processes)
      {
        auto* midi = qobject_cast<const Midi::ProcessModel*>(&proc);
        if(!midi)
          continue;
        const QPointer<const Process::ProcessModel> into = midi;
        const QString name = midi->prettyName();
        for(bool replace : {false, true})
        {
          auto act = sub->addAction(
              replace ? QObject::tr("Record replacing the notes of %1").arg(name)
                      : QObject::tr("Record into %1").arg(name));
          QObject::connect(act, &QAction::triggered, plug, [=] {
            auto opt = plug->defaultOptions();
            opt.target = RecordOptions::Target::Into;
            opt.into = into;
            opt.replace = replace;
            record(opt);
          });
        }
      }
    }
  }
  // Values into the automations of its box, replacing their curve where
  // recorded; they are silent while the take records
  if(type == Process::PortType::Message)
  {
    if(auto* box = Scenario::closestParentInterval(Process::parentProcess(&port)))
    {
      for(const auto& proc : box->processes)
      {
        auto* autom = qobject_cast<const Automation::ProcessModel*>(&proc);
        if(!autom)
          continue;
        const QPointer<const Process::ProcessModel> into = autom;
        auto act = sub->addAction(QObject::tr("Record into %1").arg(autom->prettyName()));
        QObject::connect(act, &QAction::triggered, plug, [=] {
          auto opt = plug->defaultOptions();
          opt.target = RecordOptions::Target::Into;
          opt.into = into;
          opt.replace = true;
          record(opt);
        });
      }
    }
  }
  {
    auto act = sub->addAction(QObject::tr("Record only the file"));
    act->setToolTip(QObject::tr("The take is written to Takes but not placed in the box"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.target = RecordOptions::Target::FileOnly;
      record(opt);
    });
  }
}
}
