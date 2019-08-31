// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"

#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>
#include <Process/Commands/Properties.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SpinBoxes.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
namespace Process
{
InspectorWidgetDelegateFactory::~InspectorWidgetDelegateFactory() = default;

QWidget* InspectorWidgetDelegateFactory::make(
    const InspectedObjects& objects,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  if (objects.empty())
    return nullptr;

  auto obj = objects.first();
  if (auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return make_process(*p, doc, parent);
  }
  return nullptr;
}

bool InspectorWidgetDelegateFactory::matches(
    const InspectedObjects& objects) const
{
  if (objects.empty())
    return false;

  auto obj = objects.first();
  if (auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return matches_process(*p);
  }
  return false;
}

class InspectorWidget
    : public QWidget
{
public:
  InspectorWidget(const ProcessModel& process, const score::DocumentContext& doc, QWidget* w, QWidget* parent)
    : QWidget{parent}
  {
    auto lay = new QVBoxLayout{this};

    auto label = new TextLabel{process.prettyShortName(), this};
    label->setStyleSheet("font-weight: bold; font-size: 18");
    lay->addWidget(label);
    if(!(process.flags() & ProcessFlags::TimeIndependent))
    {
      auto loop_w = new QWidget;
      auto loop_lay = new QFormLayout(loop_w);
      loop_lay->setContentsMargins(1,1,1,1);
      loop_lay->setSpacing(2);
      lay->addWidget(loop_w);

      // Loops
      {
        auto cb = new QCheckBox;
        cb->setChecked(process.loops());
        connect(cb, &QCheckBox::toggled,
                this, [&] (bool b) {
          if(b != process.loops())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoop>(process, b);
        });
        con(process, &ProcessModel::loopsChanged,
            this, [cb] (bool b) {
          if(b != cb->isChecked())
            cb->setChecked(b);
        });
        loop_lay->addRow(tr("Loops"), cb);
      }

      // Start offset
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime(QTime{0,0,0,0});
        so->setTime(process.startOffset().toQTime());
        connect(so, &score::TimeSpinBox::timeChanged,
                this, [&] (const QTime& time) {
          auto t = TimeVal{time};

          if(t != process.startOffset())
            CommandDispatcher<>{doc.commandStack}.submit<SetStartOffset>(process, t);
        });
        con(process, &ProcessModel::startOffsetChanged,
            this, [so] (TimeVal t) {
          auto tm = t.toQTime();
          if(tm != so->time())
            so->setTime(tm);
        });
        loop_lay->addRow(tr("Start offset"), so);
      }

      // Loop duration
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime(QTime{0, 0, 0, 10});
        so->setTime(process.loopDuration().toQTime());
        connect(so, &score::TimeSpinBox::timeChanged,
                this, [&] (const QTime& time) {
          auto t = TimeVal{time};

          if(t != process.loopDuration())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoopDuration>(process, t);
        });
        con(process, &ProcessModel::loopDurationChanged,
            this, [so] (TimeVal t) {
          auto tm = t.toQTime();
          if(tm != so->time())
            so->setTime(tm);
        });
        loop_lay->addRow(tr("Loop duration"), so);
      }
    }

    auto ports = new PortListWidget{process, doc, this};
    auto tab = new QTabWidget;
    tab->setTabPosition(QTabWidget::South);
    tab->addTab(w, "Basic");
    tab->addTab(ports, "Ports");
    lay->addWidget(tab);
  }
};

QWidget* InspectorWidgetDelegateFactory::wrap(
    const ProcessModel& process,
    const score::DocumentContext& doc,
    QWidget* w,
    QWidget* parent)
{
  auto widg = new InspectorWidget{process, doc, w, parent};

  return widg;
}
}
