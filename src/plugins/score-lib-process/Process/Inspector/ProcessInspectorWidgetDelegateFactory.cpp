// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Effect/EffectLayer.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/TimeSpinBox.hpp>

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

class InspectorWidget : public QWidget
{
  QPointer<ProcessModel> m_proc;
public:
  InspectorWidget(
      const ProcessModel& process,
      const score::DocumentContext& doc,
      QWidget* w,
      QWidget* parent)
      : QWidget{parent}
      , m_proc{&const_cast<Process::ProcessModel&>(process)}
  {
    auto lay = new QVBoxLayout{this};

    auto label = new TextLabel{process.metadata().getName(), this};
    auto f = label->font();
    f.setBold(true);
    f.setPixelSize(18);
    label->setFont(f);
    lay->addWidget(label);
    QWidget* loop_w{};
    QFormLayout* loop_lay{};
    QHBoxLayout* btn_lay{};

    auto initButtonsLayout = [&] {
      loop_w = new QWidget;
      loop_lay = new QFormLayout(loop_w);
      loop_lay->setContentsMargins(1, 1, 1, 1);
      loop_lay->setSpacing(2);
      lay->addWidget(loop_w);

      btn_lay = new QHBoxLayout;
      loop_lay->addRow(btn_lay);
    };
    if (!(process.flags() & ProcessFlags::TimeIndependent))
    {
      initButtonsLayout();

      // Loops
      {
        auto loop_btn = new QToolButton;
        loop_btn->setIcon(makeIcons(
            QStringLiteral(":/icons/loop_on.png"),
            QStringLiteral(":/icons/loop_hover.png"),
            QStringLiteral(":/icons/loop_off.png"),
            QStringLiteral(":/icons/loop_off.png")));
        loop_btn->setToolTip(tr("Loop"));
        loop_btn->setStatusTip(tr("Enable looping for this process"));

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
        btn_lay->addWidget(loop_btn);
      }

      // Start offset
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime({});
        so->setTime(process.startOffset());
        connect(
            so,
            &score::TimeSpinBox::timeChanged,
            this,
            [&](const ossia::time_value& t) {
              if (t != process.startOffset())
                CommandDispatcher<>{doc.commandStack}.submit<SetStartOffset>(
                    process, t);
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
        connect(
            so,
            &score::TimeSpinBox::timeChanged,
            this,
            [&](const ossia::time_value& t) {
              if (t != process.loopDuration())
                CommandDispatcher<>{doc.commandStack}.submit<SetLoopDuration>(
                    process, t);
            });
        con(process,
            &ProcessModel::loopDurationChanged,
            this,
            [so](TimeVal t) {
              if (t != so->time())
                so->setTime(t);
            });
        loop_lay->addRow(tr("Loop duration"), so);
      }
    }

    auto& processes = doc.app.interfaces<Process::LayerFactoryList>();

    if(auto fact = processes.get(process.concreteKey()); fact && fact->hasExternalUI(process, doc))
    {
      if(!loop_lay)
        initButtonsLayout();

      auto uiToggle = new QToolButton{};
      uiToggle->setIcon(makeIcons(
          QStringLiteral(":/icons/new_window_on.png"),
          QStringLiteral(":/icons/new_window_hover.png"),
          QStringLiteral(":/icons/new_window_off.png"),
          QStringLiteral(":/icons/new_window_off.png")));
      uiToggle->setToolTip(tr("Edit process"));
      uiToggle->setStatusTip(tr("Show the custom user interface of this plug-in"));
      uiToggle->setAutoRaise(true);
      uiToggle->setIconSize(QSize{28, 28});
      uiToggle->setCheckable(true);
      uiToggle->setChecked(bool(process.externalUI));

      connect(uiToggle, &QToolButton::toggled,
              this, [&process, fact, &doc] (bool state) {
        Process::setupExternalUI(process, *fact, doc, state);
      });
      connect(&process, &ProcessModel::externalUIVisible,
              uiToggle, [=] (bool v) {
        QSignalBlocker block{uiToggle};
        uiToggle->setChecked(v);
      });

      btn_lay->addWidget(uiToggle);
    }

    if (process.flags() & ProcessFlags::CanCreateControls)
    {
      if(!loop_lay)
        initButtonsLayout();
      auto controlsToggle = new QToolButton{};
      controlsToggle->setIcon(makeIcons(
          QStringLiteral(":/icons/control_record_on.png"),
          QStringLiteral(":/icons/control_record_hover.png"),
          QStringLiteral(":/icons/control_record_off.png"),
          QStringLiteral(":/icons/control_record_off.png")));
      controlsToggle->setToolTip(tr("Edit controls"));
      controlsToggle->setStatusTip(tr("Enable the controls changed in the plug-in ui to appear in the main window"));
      controlsToggle->setAutoRaise(true);
      controlsToggle->setIconSize(QSize{28, 28});
      controlsToggle->setCheckable(true);
      controlsToggle->setChecked(false);
      connect(controlsToggle, &QToolButton::toggled,
              this, [&process] (bool state) {
        auto& p = const_cast<Process::ProcessModel&>(process);
        p.setCreatingControls(state);
      });
      btn_lay->addWidget(controlsToggle);
    }

    if (w)
    {
      lay->addWidget(w);
      lay->addStretch(100);
    }
    else
    {
      auto scroll = new QScrollArea{};
      scroll->setFrameShape(QFrame::NoFrame);
      scroll->setSizePolicy(
          QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
      scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      scroll->setWidgetResizable(true);
      scroll->setWidget(new PortListWidget{process, doc, this});
      lay->addWidget(scroll);
    }
  }

  ~InspectorWidget()
  {
    if(m_proc)
    {
      m_proc->setCreatingControls(false);
    }
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
