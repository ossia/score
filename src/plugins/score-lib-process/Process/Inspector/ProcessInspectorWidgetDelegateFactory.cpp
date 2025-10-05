// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Effect/EffectLayer.hpp>
#include <Inspector/InspectorLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>
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
void InspectorWidgetDelegateFactory::addButtons(
    const Process::ProcessModel&, const score::DocumentContext& doc, QBoxLayout* layout,
    QWidget* parent) const
{
}

bool InspectorWidgetDelegateFactory::matchesProcess(
    const ProcessModel& proc, const score::DocumentContext& doc, QWidget* parent) const
{
  return matchesProcess(proc);
}

QWidget* InspectorWidgetDelegateFactory::make(
    const InspectedObjects& objects, const score::DocumentContext& doc,
    QWidget* parent) const
{
  if(objects.empty())
    return nullptr;

  auto obj = objects.first();
  if(auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return makeProcess(*p, doc, parent);
  }
  return nullptr;
}

bool InspectorWidgetDelegateFactory::matches(const InspectedObjects& objects) const
{
  if(objects.empty())
    return false;

  auto obj = objects.first();
  if(auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return matchesProcess(*p);
  }
  return false;
}
namespace
{
class InspectorWidget : public QWidget
{
  QPointer<ProcessModel> m_proc;

public:
  QHBoxLayout* m_buttons{};
  InspectorWidget(
      ProcessModel& process, const score::DocumentContext& doc, QWidget* w,
      QWidget* parent)
      : QWidget{parent}
      , m_proc{&process}
  {
    setObjectName("Process::InspectorWidget");
    auto lay = new Inspector::VBoxLayout{this};

    const auto& process_factories = doc.app.interfaces<Process::ProcessFactoryList>();
    const auto fact = process_factories.get(process.concreteKey());

    const auto proc_name = process.prettyName();
    const auto type_name = fact->prettyName();
    const auto label_text
        = proc_name.contains(type_name)
              ? QStringLiteral("Process (%1)").arg(proc_name)
              : QStringLiteral("Process (%1: %2)").arg(type_name, proc_name);
    auto label = new TextLabel{label_text, this};
    auto f = label->font();
    f.setBold(true);
    f.setPixelSize(12); // See InspectorWidgetBase
    label->setFont(f);
    lay->addWidget(label);

    QWidget* loop_w{};
    QFormLayout* loop_lay{};

    auto initButtonsLayout = [&] {
      m_buttons = new QHBoxLayout;
      m_buttons->setContentsMargins(0, 0, 0, 0);
      lay->addLayout(m_buttons);

      loop_w = new QWidget;
      loop_lay = new QFormLayout(loop_w);
      loop_lay->setContentsMargins(1, 1, 1, 1);
      loop_lay->setSpacing(2);
      lay->addWidget(loop_w);
    };

    if(!(process.flags() & ProcessFlags::TimeIndependent))
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
        score::setHelp(loop_btn, tr("Enable looping for this process"));
        loop_btn->setToolTip(tr("Loop"));

        loop_btn->setAutoRaise(true);
        loop_btn->setIconSize(QSize{28, 28});
        loop_btn->setCheckable(true);
        loop_btn->setChecked(process.loops());
        connect(loop_btn, &QToolButton::toggled, this, [&](bool b) {
          if(b != process.loops())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoop>(process, b);
        });
        con(process, &ProcessModel::loopsChanged, this, [loop_btn](bool b) {
          if(b != loop_btn->isChecked())
            loop_btn->setChecked(b);
        });
        m_buttons->addWidget(loop_btn);
      }

      // Start offset
      {
        auto so = new score::TimeSpinBox;
        so->setMinimumTime({});
        so->setTime(process.startOffset());
        connect(
            so, &score::TimeSpinBox::timeChanged, this, [&](const ossia::time_value& t) {
              if(t != process.startOffset())
                CommandDispatcher<>{doc.commandStack}.submit<SetStartOffset>(process, t);
            });
        con(process, &ProcessModel::startOffsetChanged, this, [so](TimeVal t) {
          if(t != so->time())
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
            so, &score::TimeSpinBox::timeChanged, this,
            [&](const ossia::time_value& t) {
          if(t != process.loopDuration())
            CommandDispatcher<>{doc.commandStack}.submit<SetLoopDuration>(process, t);
            });
        con(process, &ProcessModel::loopDurationChanged, this, [so](TimeVal t) {
          if(t != so->time())
            so->setTime(t);
        });
        loop_lay->addRow(tr("Loop duration"), so);
      }
    }

    auto& layer_factories = doc.app.interfaces<Process::LayerFactoryList>();

    if(auto fact = layer_factories.get(process.concreteKey());
       fact && fact->hasExternalUI(process, doc))
    {
      if(!loop_lay)
        initButtonsLayout();

      auto uiToggle = new QToolButton{};
      uiToggle->setIcon(makeIcons(
          QStringLiteral(":/icons/new_window_on.png"),
          QStringLiteral(":/icons/new_window_hover.png"),
          QStringLiteral(":/icons/new_window_off.png"),
          QStringLiteral(":/icons/new_window_off.png")));
      score::setHelp(uiToggle, tr("Show the custom user interface of this plug-in"));
      uiToggle->setToolTip(tr("Open plug-in"));
      uiToggle->setAutoRaise(true);
      uiToggle->setIconSize(QSize{28, 28});
      uiToggle->setCheckable(true);
      uiToggle->setChecked(bool(process.externalUI));

      connect(uiToggle, &QToolButton::toggled, this, [&process, fact, &doc](bool state) {
        Process::setupExternalUI(process, *fact, doc, state);
      });
      connect(&process, &ProcessModel::externalUIVisible, uiToggle, [=](bool v) {
        QSignalBlocker block{uiToggle};
        uiToggle->setChecked(v);
      });

      m_buttons->addWidget(uiToggle);
    }

    if(process.flags() & ProcessFlags::CanCreateControls)
    {
      if(!loop_lay)
        initButtonsLayout();
      auto controlsToggle = new QToolButton{};
      controlsToggle->setIcon(makeIcons(
          QStringLiteral(":/icons/control_record_on.png"),
          QStringLiteral(":/icons/control_record_hover.png"),
          QStringLiteral(":/icons/control_record_off.png"),
          QStringLiteral(":/icons/control_record_off.png")));
      score::setHelp(
          controlsToggle, tr("Enable the controls changed in the plug-in ui to appear "
                             "in the main window"));
      controlsToggle->setToolTip(tr("Edit controls"));
      controlsToggle->setAutoRaise(true);
      controlsToggle->setIconSize(QSize{28, 28});
      controlsToggle->setCheckable(true);
      controlsToggle->setChecked(false);
      connect(controlsToggle, &QToolButton::toggled, this, [&process](bool state) {
        auto& p = const_cast<Process::ProcessModel&>(process);
        p.setCreatingControls(state);
      });
      m_buttons->addWidget(controlsToggle);
    }

    // Custom widget
    // FIXME to be removed, eventually
    if(w)
      lay->addWidget(w);

    // List of ports
    auto scroll = new QScrollArea{};
    scroll->setObjectName("PortListWidgetScrollArea");
    scroll->setFrameShape(QFrame::NoFrame);
    QSizePolicy sz;
    sz.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
    sz.setVerticalPolicy(QSizePolicy::MinimumExpanding);
    sz.setVerticalStretch(255);
    scroll->setSizePolicy(sz);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidgetResizable(true);
    scroll->setWidget(new PortListWidget{process, doc, this});
    lay->addWidget(scroll);
  }

  ~InspectorWidget()
  {
    if(m_proc)
    {
      m_proc->setCreatingControls(false);
    }
  }
};
}

QWidget* InspectorWidgetDelegateFactory::wrap(
    ProcessModel& process, const score::DocumentContext& doc, QWidget* w,
    QWidget* parent) const
{
  auto widg = new InspectorWidget{process, doc, w, parent};
  addButtons(process, doc, widg->m_buttons, widg);
  if(widg->m_buttons)
    widg->m_buttons->addStretch(1);
  return widg;
}

}
