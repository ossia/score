#include "RecordMenus.hpp"

#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Record/RecordTools.hpp>

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
  {
    auto act = sub.addAction(QObject::tr("Record now"));
    QObject::connect(act, &QAction::triggered, &plug, [=] { record(pplug->defaultOptions()); });
  }
  {
    auto act = sub.addAction(QObject::tr("Record from the next bar"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = pplug->defaultOptions();
      opt.startRate = 1.;
      record(opt);
    });
  }
  {
    auto act = sub.addAction(QObject::tr("Record from the next bar, counting in"));
    act->setToolTip(QObject::tr("The beats are clicked on the main output until it starts"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = pplug->defaultOptions();
      opt.startRate = 1.;
      opt.countIn = true;
      record(opt);
    });
  }
  {
    // A number of bars from the next one: a loop that long
    auto bars = sub.addMenu(QObject::tr("Record bars from the next bar"));
    bars->setToolTipsVisible(true);
    const auto record_bars = [=](int n) {
      auto opt = pplug->defaultOptions();
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
        auto opt = pplug->defaultOptions();
        opt.startRate = 1.;
        opt.lengthBars = 0.;
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
      auto opt = pplug->defaultOptions();
      opt.start = RecordOptions::Start::FirstEvent;
      record(opt);
    });
  }
  {
    auto act = sub.addAction(QObject::tr("Record when playback starts"));
    QObject::connect(act, &QAction::triggered, &plug, [=] {
      auto opt = pplug->defaultOptions();
      opt.start = RecordOptions::Start::OnPlay;
      record(opt);
    });
  }
}

//! The entries recording the addresses of the explorer into `box`
void addAddressEntries(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug, Scenario::IntervalModel& box,
    std::vector<State::Address> addresses, std::optional<QPointF> pos,
    const score::DocumentContext& ctx)
{
  AddressKinds kinds;
  if(auto* devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
    kinds = addressKinds(*devices, addresses);
  const QPointer<Scenario::IntervalModel> target = &box;
  auto* pplug = &plug;
  const auto recordAs = [=](AddressPlacement placement) {
    return [=](RecordOptions opt) {
      if(target)
        pplug->recordAddressesInto(*target, addresses, placement, pos, opt);
    };
  };
  const RecordMenuSources sources{.events = kinds.values || kinds.midi, .notes = kinds.midi && !kinds.values};
  addTimingEntries(sub, root, plug, recordAs(AddressPlacement::Automations), sources);
  if(kinds.values)
  {
    sub.addSeparator();
    auto msgs = sub.addMenu(QObject::tr("Values as messages"));
    msgs->setToolTipsVisible(true);
    msgs->setToolTip(QObject::tr(
        "The values in states of a scenario, one where each changed, rather than in "
        "automations"));
    addTimingEntries(*msgs, root, plug, recordAs(AddressPlacement::Messages), sources);
  }
}
}
