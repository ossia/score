#include "RecordPortMenu.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessContext.hpp>

#include <Recording/ApplicationPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QMenu>

namespace Recording
{
void RecordPortMenu::setupMenu(
    QMenu& menu, const Process::Port& port, const Process::Context& ctx)
{
  const auto type = port.type();
  if(type != Process::PortType::Audio && type != Process::PortType::Midi
     && type != Process::PortType::Message)
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
  {
    auto act = sub->addAction(QObject::tr("Record from the next bar"));
    QObject::connect(act, &QAction::triggered, plug, [=] {
      auto opt = plug->defaultOptions();
      opt.startRate = 1.;
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
