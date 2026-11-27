#include <score/application/ApplicationContext.hpp>
#include <QSlider>
#include <QLabel>
#include <QActionGroup>
#include <Recording/Settings/RecordingSettings.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include "RecordMenus.hpp"

#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Record/RecordTools.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/document/DocumentContext.hpp>

#include <QPointer>

#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QWidgetAction>

namespace Recording
{
void addTimingEntries(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug,
    std::function<void(RecordOptions)> record, RecordMenuSources sources)
{
  auto* pplug = &plug;
  // Each entry says when it starts and stops: the settings' timing is what
  // the Record button does, and is not mixed into them
  const auto options = [pplug] {
    auto opt = pplug->defaultOptions();
    opt.start = RecordOptions::Start::Now;
    opt.startRate = 0.;
    opt.countIn = false;
    opt.length = 0;
    opt.lengthBars = 0.;
    opt.passes = RecordOptions::Passes::One;
    return opt;
  };
  {
    auto act = sub.addAction(QObject::tr("Record now"));
    QObject::connect(act, &QAction::triggered, &plug, [=] { record(options()); });
  }
  {
    auto act = sub.addAction(QObject::tr("Record from the next bar"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = options();
      opt.startRate = 1.;
      record(opt);
    });
  }
  {
    auto act = sub.addAction(QObject::tr("Record from the next bar, counting in"));
    act->setToolTip(QObject::tr("The beats are clicked on the main output until it starts"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = options();
      opt.startRate = 1.;
      opt.countIn = true;
      record(opt);
    });
  }
  if(sources.bars)
  {
    // A number of bars from the next one: a loop that long
    auto bars = sub.addMenu(QObject::tr("Record bars from the next bar"));
    bars->setToolTipsVisible(true);
    const auto record_bars = [=](int n) {
      auto opt = options();
      opt.startRate = 1.;
      opt.lengthBars = n;
      record(opt);
    };
    for(int n : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 20, 24, 28, 32, 64})
    {
      auto act = bars->addAction(
          n == 1 ? QObject::tr("1 bar") : QObject::tr("%1 bars").arg(n));
      QObject::connect(act, &QAction::triggered, &plug, [=] { record_bars(n); });
    }
    bars->addSeparator();
    {
      // Any other number
      auto w = new QWidget{bars};
      auto lay = new QHBoxLayout{w};
      lay->setContentsMargins(6, 2, 6, 2);
      auto spin = new QSpinBox{w};
      spin->setRange(1, 1024);
      spin->setValue(16);
      spin->setSuffix(QObject::tr(" bars"));
      auto go = new QPushButton{QObject::tr("Record"), w};
      lay->addWidget(spin);
      lay->addWidget(go);
      auto wa = new QWidgetAction{bars};
      wa->setDefaultWidget(w);
      bars->addAction(wa);
      const auto start = [=, &root] {
        const int n = spin->value();
        root.close();
        record_bars(n);
      };
      QObject::connect(go, &QPushButton::clicked, &plug, start);
      QObject::connect(spin, &QSpinBox::editingFinished, &plug, [=] {
        if(spin->hasFocus())
          start();
      });
    }
  }
  // Over and over, a bar at a time: the passes are kept as the user chooses
  if(sources.bars && sources.passes)
  {
    auto passes = sub.addMenu(QObject::tr("Record passes of one bar"));
    using P = RecordOptions::Passes;
    const std::pair<P, QString> kinds[]{
        {P::Alternatives, QObject::tr("As alternatives, the last one playing")},
        {P::SideBySide, QObject::tr("Side by side, where they were played")},
        {P::LastOnly, QObject::tr("Keeping only the last one")}};
    for(const auto& [kind, text] : kinds)
    {
      auto act = passes->addAction(text);
      QObject::connect(act, &QAction::triggered, &plug, [=, kind = kind] {
        auto opt = options();
        opt.startRate = 1.;
        opt.passes = kind;
        opt.passBars = 1.;
        record(opt);
      });
    }
  }
  if(sources.events)
  {
    auto act = sub.addAction(
        sources.notes ? QObject::tr("Record from the first note")
                      : QObject::tr("Record from the first value"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = options();
      opt.start = RecordOptions::Start::FirstEvent;
      record(opt);
    });
  }
  {
    auto act = sub.addAction(QObject::tr("Record when playback starts"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = options();
      opt.start = RecordOptions::Start::OnPlay;
      record(opt);
    });
  }
  if(sources.files)
  {
    auto act = sub.addAction(QObject::tr("Record only the file"));
    act->setToolTip(QObject::tr("The take is written to Takes but not placed"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = options();
      opt.target = RecordOptions::Target::FileOnly;
      record(opt);
    });
  }
}

namespace
{
using AddressRecorder = std::function<void(AddressPlacement, RecordOptions)>;

//! The timing entries for addresses, values as automations (MIDI devices as
//! notes, sound and video inputs as files), and the same with values as messages
void addAddressTiming(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug, AddressKinds kinds,
    AddressRecorder rec)
{
  const auto as = [=](AddressPlacement placement) {
    return [=](RecordOptions opt) { rec(placement, opt); };
  };
  // Values are recorded into curves or states: no file of their own, and
  // one curve, not passes of it
  const RecordMenuSources sources{
      .events = kinds.values || kinds.midi,
      .notes = kinds.midi && !kinds.values,
      .bars = true,
      .passes = !kinds.values,
      .files = !kinds.values};
  addTimingEntries(sub, root, plug, as(AddressPlacement::Automations), sources);
  if(kinds.values)
  {
    sub.addSeparator();
    auto msgs = sub.addMenu(QObject::tr("Values as messages"));
    msgs->setToolTipsVisible(true);
    msgs->setToolTip(QObject::tr(
        "The values in states of a scenario, one where each changed, rather than in "
        "automations"));
    addTimingEntries(*msgs, root, plug, as(AddressPlacement::Messages), sources);
  }
}

AddressKinds kindsOf(
    const score::DocumentContext& ctx, const std::vector<State::Address>& addresses)
{
  if(auto* devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
    return addressKinds(*devices, addresses);
  return {};
}
}

void addAddressEntries(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug, Scenario::IntervalModel& box,
    std::vector<State::Address> addresses, std::optional<QPointF> pos,
    const score::DocumentContext& ctx)
{
  const QPointer<Scenario::IntervalModel> target = &box;
  auto* pplug = &plug;
  addAddressTiming(
      sub, root, plug, kindsOf(ctx, addresses),
      [=](AddressPlacement placement, RecordOptions opt) {
    if(target)
      pplug->recordAddressesInto(*target, addresses, placement, pos, opt);
  });
}

void addCountInEntries(QMenu& menu, const score::ApplicationContext& ctx)
{
  auto* set = ctx.findSettings<Settings::Model>();
  if(!set)
    return;
  {
    auto act = menu.addAction(QObject::tr("Metronome"));
    act->setCheckable(true);
    act->setChecked(set->getMetronome());
    act->setToolTip(QObject::tr("Click the beats on the cue output while playing"));
    QObject::connect(act, &QAction::toggled, set, [set](bool b) { set->setMetronome(b); });
  }
  {
    auto act = menu.addAction(QObject::tr("Count in before playback"));
    act->setCheckable(true);
    act->setChecked(set->getCountInOnPlay());
    act->setToolTip(QObject::tr("Play waits for the count-in's bars of clicks"));
    QObject::connect(
        act, &QAction::toggled, set, [set](bool b) { set->setCountInOnPlay(b); });
  }
  auto sub = menu.addMenu(QObject::tr("Count-in"));
  sub->setToolTipsVisible(true);
  sub->setToolTip(QObject::tr(
      "How the metronome and the count-in click on the cue output"));

  // Its length
  {
    auto group = new QActionGroup{sub};
    for(int n : {1, 2, 4})
    {
      auto act = sub->addAction(
          n == 1 ? QObject::tr("1 bar") : QObject::tr("%1 bars").arg(n));
      act->setCheckable(true);
      act->setChecked(set->getCountInBars() == n);
      group->addAction(act);
      QObject::connect(act, &QAction::triggered, set, [set, n] { set->setCountInBars(n); });
    }
  }
  sub->addSeparator();

  // Its sound
  {
    auto sounds = sub->addMenu(QObject::tr("Sound"));
    auto group = new QActionGroup{sounds};
    const QStringList names = Settings::ClickSounds{};
    for(const QString& name : names)
    {
      auto act = sounds->addAction(QObject::tr(name.toUtf8().constData()));
      act->setCheckable(true);
      act->setChecked(set->getClickSound() == name);
      group->addAction(act);
      QObject::connect(act, &QAction::triggered, set, [set, name] { set->setClickSound(name); });
    }
  }

  // Its volume
  {
    auto w = new QWidget{sub};
    auto lay = new QHBoxLayout{w};
    lay->setContentsMargins(6, 2, 6, 2);
    lay->addWidget(new QLabel{QObject::tr("Volume"), w});
    auto slider = new QSlider{Qt::Horizontal, w};
    slider->setRange(0, 100);
    slider->setValue(set->getClickVolume());
    slider->setMinimumWidth(100);
    lay->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, set, [set](int v) { set->setClickVolume(v); });
    auto wa = new QWidgetAction{sub};
    wa->setDefaultWidget(w);
    sub->addAction(wa);
  }

  {
    auto act = sub->addAction(QObject::tr("Accent the first beat"));
    act->setCheckable(true);
    act->setChecked(set->getClickAccent());
    QObject::connect(act, &QAction::toggled, set, [set](bool b) { set->setClickAccent(b); });
  }
}

void targetOfSelection(
    Scenario::ProcessModel& scenario, const Selection& selection, RecordMenuTarget& target)
{
  if(selection.size() != 1)
    return;
  QObject* obj = selection.begin()->data();
  if(!obj || obj->parent() != &scenario)
    return;
  const auto firstState = [&](const Scenario::EventModel& ev) -> const Scenario::StateModel* {
    if(ev.states().empty())
      return nullptr;
    auto it = scenario.states.find(ev.states().front());
    return it != scenario.states.end() ? &*it : nullptr;
  };
  if(auto* itv = qobject_cast<Scenario::IntervalModel*>(obj))
  {
    target.box = itv;
  }
  else if(auto* st = qobject_cast<Scenario::StateModel*>(obj))
  {
    target.after = st;
  }
  else if(auto* ev = qobject_cast<Scenario::EventModel*>(obj))
  {
    target.after = firstState(*ev);
  }
  else if(auto* ts = qobject_cast<Scenario::TimeSyncModel*>(obj))
  {
    if(!ts->events().empty())
      if(auto it = scenario.events.find(ts->events().front()); it != scenario.events.end())
        target.after = firstState(*it);
  }
}

void addRecordEntries(
    QMenu& menu, ApplicationPlugin& plug, const score::DocumentContext& ctx,
    RecordMenuTarget target)
{
  auto* pplug = &plug;
  if(plug.recording(ctx))
  {
    auto stop = menu.addAction(QObject::tr("Stop recording"));
    QObject::connect(stop, &QAction::triggered, &plug, [pplug] { pplug->stopSession(); });
    return;
  }

  auto sub = menu.addMenu(QObject::tr("Record"));
  sub->setToolTipsVisible(true);
  // One recording at a time, in the whole application
  if(plug.session())
  {
    sub->addAction(QObject::tr("Recording in another document"))->setEnabled(false);
    return;
  }

  // The processes selected, else those armed: what the Record button records
  {
    bool armed{};
    const auto ports = plug.portsToRecord(ctx, &armed);
    if(ports.empty())
    {
      sub->addAction(QObject::tr("Select or arm processes to record them"))
          ->setEnabled(false);
    }
    else
    {
      auto procs = sub->addMenu(
          armed ? QObject::tr("The armed processes")
                : QObject::tr("The selected processes"));
      procs->setToolTipsVisible(true);
      bool midi = false;
      for(auto* p : ports)
        midi = midi || p->type() == Process::PortType::Midi;
      const auto pos = target.nodePosition;
      const auto* pctx = &ctx;
      const QPointer<const Scenario::IntervalModel> into = target.box;
      const QPointer<const Scenario::StateModel> after = target.after;
      if(target.box)
        procs->setToolTip(QObject::tr("Recorded into this box"));
      else if(target.after)
        procs->setToolTip(QObject::tr("Recorded into a new box after this state"));
      addTimingEntries(
          *procs, menu, plug,
          [=](RecordOptions opt) {
        opt.takesInto = into;
        opt.takesAfter = after;
        pplug->recordSelection(*pctx, pos, opt);
      },
          {.events = midi, .notes = true, .bars = true, .files = true});
    }
  }

  // The addresses selected in the explorer
  {
    const auto addresses = plug.selectedAddresses(ctx);
    if(addresses.empty())
    {
      sub->addAction(QObject::tr("Select devices or addresses in the explorer to record them"))
          ->setEnabled(false);
    }
    else
    {
      auto addr = sub->addMenu(QObject::tr("The selected addresses"));
      addr->setToolTipsVisible(true);
      addr->setToolTip(
          target.box     ? QObject::tr("Recorded into this box")
          : target.after ? QObject::tr("Recorded into a new box after this state")
                         : QObject::tr("Recorded into a new box where the menu was opened"));
      const QPointer<Scenario::IntervalModel> box = target.box;
      const QPointer<const Scenario::StateModel> after = target.after;
      const bool hasAfter = target.after != nullptr;
      const QPointer<Scenario::ProcessModel> scenario = target.scenario;
      const auto pt = target.point;
      const auto pos = target.nodePosition;
      addAddressTiming(
          *addr, menu, plug, kindsOf(ctx, addresses),
          [=](AddressPlacement placement, RecordOptions opt) {
        if(box)
          pplug->recordAddressesInto(*box, addresses, placement, pos, opt);
        else if(scenario && (!hasAfter || after))
          pplug->recordAddresses(*scenario, pt, addresses, placement, opt, after.data());
      });
    }
  }

  // What the armed processes just played
  if(plug.captureSession())
  {
    sub->addSeparator();
    auto cap = sub->addAction(QObject::tr("Capture the last bars played"));
    QObject::connect(cap, &QAction::triggered, &plug, [pplug] { pplug->capture(); });
  }
}
}
