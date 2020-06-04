// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QToolButton>
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

bool InspectorWidgetDelegateFactory::matches(const InspectedObjects& objects) const
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

class InspectorWidget : public QWidget
{
public:
  InspectorWidget(
      const ProcessModel& process,
      const score::DocumentContext& doc,
      QWidget* w,
      QWidget* parent)
      : QWidget{parent}
  {
    auto lay = new QVBoxLayout{this};

    auto label = new TextLabel{process.prettyShortName(), this};
    auto f = label->font();
    f.setBold(true);
    f.setPixelSize(18);
    label->setFont(f);
    lay->addWidget(label);
    if (!(process.flags() & ProcessFlags::TimeIndependent))
    {
      auto loop_w = new QWidget;
      auto loop_lay = new QFormLayout(loop_w);
      loop_lay->setContentsMargins(1, 1, 1, 1);
      loop_lay->setSpacing(2);
      lay->addWidget(loop_w);

      // Loops
      {
        auto loop_btn = new QToolButton;
        loop_btn->setIcon(makeIcons(
            QStringLiteral(":/icons/loop_on.png"),
            QStringLiteral(":/icons/loop_hover.png"),
            QStringLiteral(":/icons/loop_off.png"),
            QStringLiteral(":/icons/loop_off.png")));
        loop_btn->setToolTip(tr("Loop"));

        loop_btn->setAutoRaise(true);
        loop_btn->setIconSize(QSize{28, 28});
        loop_btn->setCheckable(true);
        loop_btn->setChecked(process.loops());
        connect(loop_btn, &QToolButton::toggled, this, [&](bool b) {
          if (b != process.loops())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoop>(process, b);
        });
        con(process, &ProcessModel::loopsChanged, this, [loop_btn](bool b) {
          if (b != loop_btn->isChecked())
            loop_btn->setChecked(b);
        });
        loop_lay->addRow(loop_btn);
      }

      // Start offset
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime({});
        so->setTime(process.startOffset());
        connect(so, &score::TimeSpinBox::timeChanged, this, [&](const ossia::time_value& t) {
          if (t != process.startOffset())
            CommandDispatcher<>{doc.commandStack}.submit<SetStartOffset>(process, t);
        });
        con(process, &ProcessModel::startOffsetChanged, this, [so](TimeVal t) {
          if (t != so->time())
            so->setTime(t);
        });
        loop_lay->addRow(tr("Start offset"), so);
      }

      // Loop duration
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime(TimeVal::fromMsecs(10));
        so->setTime(process.loopDuration());
        connect(so, &score::TimeSpinBox::timeChanged, this, [&](const ossia::time_value& t) {
          if (t != process.loopDuration())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoopDuration>(process, t);
        });
        con(process, &ProcessModel::loopDurationChanged, this, [so](TimeVal t) {
          if (t != so->time())
            so->setTime(t);
        });
        loop_lay->addRow(tr("Loop duration"), so);
      }
    }

    if (w)
    {
      lay->addWidget(w);
    }
    else
    {
      auto scroll = new QScrollArea;
      scroll->setFrameShape(QFrame::NoFrame);
      scroll->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
      scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      scroll->setWidgetResizable(true);
      scroll->setWidget(new PortListWidget{process, doc, this});
      lay->addWidget(scroll);
    }
    lay->addStretch(100);
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
