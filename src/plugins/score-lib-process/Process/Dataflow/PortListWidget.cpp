#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/AddressAccessorEditWidget.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>

#include <Inspector/InspectorLayout.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/model/Skin.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/TextLabel.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortAddressComboBox.hpp>

#include <score/model/ModelMetadata.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>

#include <QCheckBox>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QSignalBlocker>
#include <QComboBox>
#include <QLineEdit>
#include <QToolButton>
namespace Process
{
PortListWidget::PortListWidget(
    const Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QWidget* parent)
    : QWidget{parent}
    , m_process{proc}
    , m_ctx{ctx}
{
  setLayout(new Inspector::Layout);
  reload();

  con(proc, &Process::ProcessModel::controlAdded, this, &PortListWidget::reload);
  con(proc, &Process::ProcessModel::controlRemoved, this, &PortListWidget::reload);
  con(proc, &Process::ProcessModel::controlOutletAdded, this, &PortListWidget::reload);
  con(proc, &Process::ProcessModel::controlOutletRemoved, this, &PortListWidget::reload);
  con(proc, &Process::ProcessModel::inletsChanged, this, &PortListWidget::reload);
  con(proc, &Process::ProcessModel::outletsChanged, this, &PortListWidget::reload);
}

void PortListWidget::reload()
{
  auto& old_lay = *(Inspector::Layout*)layout();
  auto& lay = *new Inspector::Layout;
  QWidget{}.setLayout(&old_lay);
  setLayout(&lay);

  auto& pf = m_ctx.app.interfaces<PortFactoryList>();

  if(!m_process.inlets().empty())
  {
    lay.addRow(tr("<b>Inputs</b>"), (QWidget*)nullptr);
    for(auto port : m_process.inlets())
    {
      auto fact = pf.get(port->concreteKey());
      if(fact)
      {
        fact->setupInletInspector(*port, m_ctx, this, lay, this);
      }
      else
      {
        PortWidgetSetup::setupAlone(*port, m_ctx, lay, this);
      }
    }
  }

  if(!m_process.outlets().empty())
  {
    lay.addRow(tr("<b>Outputs</b>"), (QWidget*)nullptr);
    for(auto port : m_process.outlets())
    {
      auto fact = pf.get(port->concreteKey());
      if(fact)
      {
        fact->setupOutletInspector(*port, m_ctx, this, lay, this);
      }
      else
      {
        PortWidgetSetup::setupAlone(*port, m_ctx, lay, this);
      }
    }
  }
}

AddressStatus
AddressStatus::of(const Process::Port& port, const score::DocumentContext& ctx)
{
  AddressStatus st;
  if(!port.address().isSet())
    return st;

  // Graphics nodes pull their textures and geometries from the address and
  // push them to it whatever the cables (Gfx::gfx_exec_node::run)
  switch(port.type())
  {
    case PortType::Texture:
    case PortType::Geometry:
      return st;
    default:
      break;
  }

  int glutton = 0;
  int other = 0;
  for(const auto& path : port.cables())
  {
    if(auto cable = path.try_find(ctx))
    {
      if(cable->type() == CableType::ImmediateGlutton)
        glutton++;
      else
        other++;
    }
  }

  const bool inlet = qobject_cast<const Inlet*>(&port) != nullptr;
  bool propagated = false;
  if(auto audio = qobject_cast<const AudioOutlet*>(&port))
    propagated = audio->propagate();

  if(glutton == 0 && other == 0 && !propagated)
    return st;

  if(glutton > 0)
  {
    // The engine copies the address for each immediate glutton cable whose
    // other end is not running this tick (graph_util::init_inlet,
    // graph_util::teardown_outlet)
    st.kind = WhileCablesIdle;
    st.reason
        = inlet ? QObject::tr(
                      "This input has cables: its address is only read while a "
                      "process it is connected to is not playing.")
                : QObject::tr(
                      "This output has cables: it only writes to its address while "
                      "a process it is connected to is not playing.");
    return st;
  }

  st.kind = Unused;
  if(inlet)
  {
    st.reason = QObject::tr(
        "This input only has strict or delayed cables: its address is not read. "
        "Remove the cables to read the address.");
  }
  else if(other == 0)
  {
    st.reason = QObject::tr(
        "This output is propagated to its parent: it does not write to its "
        "address. Uncheck Propagate to write to the address.");
  }
  else
  {
    st.reason = QObject::tr(
        "This output only has strict or delayed cables: it does not write to its "
        "address. Remove the cables to write to the address.");
    if(propagated)
      st.reason += QStringLiteral(" ")
                   + QObject::tr("Propagate has to be unchecked too.");
  }
  return st;
}

namespace
{
//! The closest parent of a port that has a name: a process, an interval
const IdentifiedObjectAbstract* namedOwner(const QObject* port)
{
  for(auto obj = port ? port->parent() : nullptr; obj; obj = obj->parent())
  {
    if(obj->findChild<const score::ModelMetadata*>(
           QString{}, Qt::FindDirectChildrenOnly))
      return qobject_cast<const IdentifiedObjectAbstract*>(obj);
  }
  return nullptr;
}

const score::ModelMetadata* metadataOf(const QObject& obj)
{
  return obj.findChild<const score::ModelMetadata*>(
      QString{}, Qt::FindDirectChildrenOnly);
}

QString ownerName(const IdentifiedObjectAbstract& obj)
{
  if(auto md = metadataOf(obj))
    return md->getName();
  return obj.objectName();
}

QString cableTypeName(CableType t)
{
  switch(t)
  {
    case CableType::ImmediateStrict:
      return QObject::tr("strict");
    case CableType::DelayedGlutton:
      return QObject::tr("delayed");
    case CableType::DelayedStrict:
      return QObject::tr("delayed, strict");
    case CableType::ImmediateGlutton:
    default:
      return {};
  }
}

//! Selection changes rebuild the inspector, which owns the widget that asks
//! for it: the selection is made once the click has been handled.
void selectLater(
    const score::DocumentContext& ctx, QPointer<const IdentifiedObjectAbstract> obj)
{
  QTimer::singleShot(0, &ctx.selectionStack, [&ctx, obj] {
    if(obj)
      score::SelectionDispatcher{ctx.selectionStack}.select(*obj);
  });
}

/**
 * @brief Base for the widgets that follow the cables of a port.
 *
 * Every change (a cable added or removed, its type, the name of what is at
 * the other end) schedules one refresh once the event loop is back, as these
 * signals come in the middle of commands that are still editing the model.
 */
class PortCableWatcher : public QWidget
{
public:
  PortCableWatcher(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
      , m_port{&port}
      , m_ctx{ctx}
  {
    connect(&port, &Port::cablesChanged, this, &PortCableWatcher::schedule);
    connect(&port, &Port::addressChanged, this, &PortCableWatcher::schedule);
    if(auto audio = qobject_cast<const AudioOutlet*>(&port))
      connect(audio, &AudioOutlet::propagateChanged, this, &PortCableWatcher::schedule);
  }

  void schedule()
  {
    if(m_pending)
      return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
      m_pending = false;
      refresh();
    });
  }

  void refresh()
  {
    delete m_watch;
    m_watch = new QObject{this};
    if(m_port)
      rebuild(*m_port);
  }

protected:
  virtual void rebuild(const Process::Port& port) = 0;

  //! The cables of the port, watched until the next refresh
  template <typename F>
  void forEachCable(const Process::Port& port, F&& f)
  {
    const bool inlet = qobject_cast<const Inlet*>(&port) != nullptr;
    for(const auto& path : port.cables())
    {
      auto cable = path.try_find(m_ctx);
      if(!cable)
        continue;
      connect(cable, &Cable::typeChanged, m_watch, [this] { schedule(); });

      const Port* other = inlet
                              ? static_cast<const Port*>(cable->source().try_find(m_ctx))
                              : static_cast<const Port*>(cable->sink().try_find(m_ctx));
      if(other)
      {
        connect(other, &Port::nameChanged, m_watch, [this] { schedule(); });
        connect(other, &Port::exposedChanged, m_watch, [this] { schedule(); });
        if(auto owner = namedOwner(other))
          if(auto md = metadataOf(*owner))
            connect(md, &score::ModelMetadata::NameChanged, m_watch, [this] {
              schedule();
            });
      }
      f(*cable, other);
    }
  }

  QPointer<const Process::Port> m_port;
  const score::DocumentContext& m_ctx;
  QObject* m_watch{};
  bool m_pending{};
};

class PortCablesWidget final : public PortCableWatcher
{
public:
  PortCablesWidget(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent)
      : PortCableWatcher{port, ctx, parent}
      , m_layout{new score::MarginLess<QVBoxLayout>{this}}
  {
    setObjectName(QStringLiteral("PortCables"));
    m_layout->setContentsMargins(0, 0, 0, 2);
    m_layout->setSpacing(0);
    refresh();
  }

private:
  void rebuild(const Process::Port& port) override
  {
    while(auto item = m_layout->takeAt(0))
    {
      delete item->widget();
      delete item;
    }
    const bool inlet = qobject_cast<const Inlet*>(&port) != nullptr;
    int shown = 0;
    forEachCable(port, [&](const Cable& cable, const Port* other) {
      if(!other)
        return;
      auto owner = namedOwner(other);

      auto row = new QWidget{this};
      auto hl = new score::MarginLess<QHBoxLayout>{row};
      hl->setSpacing(4);

      auto sel = new QToolButton{row};
      sel->setObjectName(QStringLiteral("SelectCable"));
      sel->setIcon(makeIcons(
          QStringLiteral(":/icons/show_cables_on.png"),
          QStringLiteral(":/icons/show_cables_hover.png"),
          QStringLiteral(":/icons/show_cables_off.png"),
          QStringLiteral(":/icons/show_cables_disabled.png")));
      sel->setIconSize(QSize{16, 16});
      sel->setAutoRaise(true);
      score::setHelp(
          sel, inlet ? tr("Select the cable coming from %1").arg(displayName(*other))
                     : tr("Select the cable going to %1").arg(displayName(*other)));
      connect(
          sel, &QToolButton::clicked, sel,
          [&ctx = m_ctx, c = QPointer<const IdentifiedObjectAbstract>{&cable}] {
        selectLater(ctx, c);
      });
      hl->addWidget(sel);

      auto text = new QLabel{row};
      text->setObjectName(QStringLiteral("PortCable"));
      text->setTextFormat(Qt::RichText);
      text->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
      QString str;
      if(owner)
        str = QStringLiteral("<a href=\"owner\">%1</a>: ")
                  .arg(ownerName(*owner).toHtmlEscaped());
      str += displayName(*other).toHtmlEscaped();
      if(auto t = cableTypeName(cable.type()); !t.isEmpty())
        str += QStringLiteral(" (%1)").arg(t);
      text->setText(str);
      if(owner)
        text->setToolTip(tr("Click to select %1").arg(ownerName(*owner)));
      connect(
          text, &QLabel::linkActivated, text,
          [&ctx = m_ctx, o = QPointer<const IdentifiedObjectAbstract>{owner}] {
        selectLater(ctx, o);
      });
      hl->addWidget(text, 1);
      m_layout->addWidget(row);
      shown++;
    });
    setVisible(shown > 0);
  }

  QVBoxLayout* m_layout{};
};

class AddressStatusIndicator final : public PortCableWatcher
{
public:
  AddressStatusIndicator(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent)
      : PortCableWatcher{port, ctx, parent}
      , m_icon{new QLabel{this}}
  {
    setObjectName(QStringLiteral("AddressStatus"));
    auto lay = new score::MarginLess<QHBoxLayout>{this};
    lay->addWidget(m_icon);
    refresh();
  }

private:
  void rebuild(const Process::Port& port) override
  {
    // Followed for their type only
    forEachCable(port, [](const Cable&, const Port*) {});

    const auto st = AddressStatus::of(port, m_ctx);
    setVisible(st.kind != AddressStatus::Used);
    if(st.kind == AddressStatus::Used)
      return;

    const auto icon = st.kind == AddressStatus::Unused
                          ? QStringLiteral(":/icons/message_warning.png")
                          : QStringLiteral(":/icons/message_information.png");
    m_icon->setPixmap(QIcon{icon}.pixmap(QSize{14, 14}, devicePixelRatioF()));
    score::setHelp(this, st.reason);
    setProperty("addressStatus", int(st.kind));
  }

  QLabel* m_icon{};
};
}

QWidget* PortWidgetSetup::makeCablesWidget(
    const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent)
{
  return new PortCablesWidget{port, ctx, parent};
}

QWidget* PortWidgetSetup::withAddressStatus(
    const Process::Port& port, QWidget* address_widget,
    const score::DocumentContext& ctx, QWidget* parent)
{
  auto widg = new QWidget{parent};
  auto lay = new score::MarginLess<QHBoxLayout>{widg};
  lay->setSpacing(2);
  address_widget->setParent(widg);
  lay->addWidget(address_widget, 1);
  lay->addWidget(new AddressStatusIndicator{port, ctx, widg});
  return widg;
}

void PortWidgetSetup::Drawer::setOpen(bool open) const
{
  widget->setVisible(open);
  button->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
}

//! Shows the name of a port as its node does, and follows it
static void showDisplayName(const Port& port, QLabel& label)
{
  label.setText(displayName(port));
  auto update = [&port, &label] { label.setText(displayName(port)); };
  QObject::connect(&port, &Port::nameChanged, &label, update);
  QObject::connect(&port, &Port::exposedChanged, &label, update);
}

PortWidgetSetup::Drawer PortWidgetSetup::setupWithDrawer(
    const Process::Port& port, QWidget* main, const score::DocumentContext& ctx,
    Inspector::Layout& lay, QWidget* parent)
{
  auto header = new QWidget;
  auto advBtn = new score::ArrowButton{Qt::RightArrow, header};
  advBtn->setObjectName(QStringLiteral("PortDrawerButton"));
  score::setHelp(advBtn, QObject::tr("Show or hide the settings of this port"));

  auto lab = new TextLabel{QString{}, header};
  showDisplayName(port, *lab);
  auto hl = new score::MarginLess<QHBoxLayout>{header};
  hl->addWidget(advBtn);
  hl->addWidget(lab);
  // The description shows in the help panel from the label and the main
  // widget too, not only from the port
  if(const auto& desc = port.description(); !desc.isEmpty())
  {
    score::setHelp(header, desc);
    score::setHelp(main, desc);
  }

  auto sw = new QWidget{parent};
  sw->setObjectName(QStringLiteral("PortDrawer"));
  sw->setContentsMargins(0, 0, 0, 0);
  auto hl2 = new score::MarginLess<QHBoxLayout>{sw};
  hl2->addSpacing(30);
  auto inner = new Inspector::Layout{};
  hl2->addLayout(inner);

  QObject::connect(advBtn, &QToolButton::clicked, sw, [sw, advBtn] {
    Drawer{sw, nullptr, advBtn}.setOpen(!sw->isVisible());
  });

  lay.addRow(header, main);
  lay.addRow(static_cast<QWidget*>(nullptr), makeCablesWidget(port, ctx, parent));

  Drawer d{sw, inner, advBtn};
  d.setOpen(false);
  return d;
}

void PortWidgetSetup::setupAddressPort(
    const Process::Port& port, const score::DocumentContext& ctx,
    Inspector::Layout& lay, QWidget* parent)
{
  auto edit = withAddressStatus(port, makePortAddressCombo(port, ctx, parent), ctx, parent);
  auto lab = new TextLabel{QString{}};
  showDisplayName(port, *lab);
  if(const auto& desc = port.description(); !desc.isEmpty())
  {
    score::setHelp(lab, desc);
    score::setHelp(edit, desc);
  }
  lay.addRow(lab, edit);
  lay.addRow(static_cast<QWidget*>(nullptr), makeCablesWidget(port, ctx, parent));
}

void PortWidgetSetup::setupAlone(
    const Port& port, const score::DocumentContext& ctx, Inspector::Layout& lay,
    QWidget* parent)
{
  auto label = setupImpl(displayName(port), port, ctx, lay, parent, true);
  showDisplayName(port, *label);
}

void PortWidgetSetup::setupInLayout(
    const Port& port, const score::DocumentContext& ctx, Inspector::Layout& lay,
    QWidget* parent)
{
  setupImpl(QObject::tr("Address"), port, ctx, lay, parent, false);
}

void PortWidgetSetup::setupControlImpl(
    const Process::Port& port, QWidget* inlet_widget, const score::DocumentContext& ctx,
    Inspector::Layout& vlay, QWidget* parent)
{
  auto drawer = setupWithDrawer(port, inlet_widget, ctx, vlay, parent);
  Process::PortWidgetSetup::setupInLayout(port, ctx, *drawer.layout, drawer.widget);
  drawer.layout->addRow(makeScriptableWidget(port, ctx, drawer.widget));
  drawer.setOpen(port.address().isSet() || port.scriptable());
  vlay.addRow(drawer.widget);
}

void PortWidgetSetup::setupControl(
    const ControlInlet& inlet, QWidget* inlet_widget, const score::DocumentContext& ctx,
    Inspector::Layout& vlay, QWidget* parent)
{
  setupControlImpl(inlet, inlet_widget, ctx, vlay, parent);
}

void PortWidgetSetup::setupControl(
    const ControlOutlet& outlet, QWidget* outlet_widget, const score::DocumentContext& ctx,
    Inspector::Layout& vlay, QWidget* parent)
{
  setupControlImpl(outlet, outlet_widget, ctx, vlay, parent);
}

QWidget* PortWidgetSetup::makeAddressWidget(
    const Port& port, const score::DocumentContext& ctx, QWidget* parent)
{
  using namespace Device;
  auto edit = new AddressAccessorEditWidget{ctx, parent};
  edit->setAddress(port.address());

  QObject::connect(
      &port, &Port::addressChanged, edit, [edit](const State::AddressAccessor& addr) {
        if(addr != edit->address().address)
        {
          edit->setAddress(addr);
        }
      });

  QObject::connect(
      edit, &AddressAccessorEditWidget::addressChanged, parent,
      [&port, &ctx](const auto& newAddr) {
    if(newAddr.address == port.address())
      return;

    CommandDispatcher<>{ctx.dispatcher}.submit(
        new Process::ChangePortAddress{port, newAddr.address});
      });

  return withAddressStatus(port, edit, ctx, parent);
}

QLabel* PortWidgetSetup::setupImpl(
    const QString& txt, const Port& port, const score::DocumentContext& ctx,
    Inspector::Layout& lay, QWidget* parent, bool withCables)
{
  auto widg = new QWidget;
  auto hl = new score::MarginLess<QHBoxLayout>{widg};

  auto advBtn = new QToolButton{widg};
  score::setSkinIconSize(advBtn, 16);
  hl->addWidget(advBtn);

  auto lab = new TextLabel{txt, widg};
  hl->addWidget(lab);

  auto port_widg = PortWidgetSetup::makeAddressWidget(port, ctx, parent);
  if(const auto& desc = port.description(); !desc.isEmpty())
  {
    score::setHelp(widg, desc);
    score::setHelp(port_widg, desc);
  }
  lay.addRow(widg, port_widg);
  if(withCables)
    lay.addRow(static_cast<QWidget*>(nullptr), makeCablesWidget(port, ctx, parent));

  switch(port.type())
  {
    case Process::PortType::Audio:
      advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_audio.png")));
      break;
    case Process::PortType::Midi:
      advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_midi.png")));
      break;
    case Process::PortType::Message:
      advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_message.png")));
      break;
    case Process::PortType::Texture:
      advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_texture.png")));
      break;
    case Process::PortType::Geometry:
      advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_texture.png")));
      break;
  }
  return lab;
}

QWidget* PortWidgetSetup::makeScriptableWidget(
    const Port& port, const score::DocumentContext& ctx, QWidget* parent)
{
  auto widg = new QWidget{parent};
  auto lay = new score::MarginLess<QHBoxLayout>{widg};

  auto check = new QCheckBox{QObject::tr("Scriptable"), widg};
  // The widget may outlive a port that its process removes
  const QPointer<const Port> guard{&port};
  // Controls of a scriptable process are always published
  auto showPublished = [check, guard] {
    if(!guard)
      return;
    const auto& port = *guard;
    const bool withProcess = LocalTree::publishedWithProcess(port);
    const QSignalBlocker block{check};
    check->setChecked(port.scriptable() || withProcess);
    check->setEnabled(!withProcess);
  };
  showPublished();
  if(auto proc = Process::parentProcess(&port))
    QObject::connect(proc, &ProcessModel::scriptableChanged, check, showPublished);
  score::setHelp(
      check, QObject::tr("Publish this control in the local device, under "
                         "score:/controls, so that states, automations, scripts "
                         "and remote controllers reach it by name."));
  QObject::connect(check, &QCheckBox::toggled, widg, [guard, &ctx](bool b) {
    if(guard && b != guard->scriptable())
      CommandDispatcher<>{ctx.commandStack}.submit<SetPortScriptable>(*guard, b);
  });
  QObject::connect(&port, &Port::scriptableChanged, check, showPublished);

  auto name = new QLineEdit{port.exposed(), widg};
  name->setPlaceholderText(QObject::tr("Scripting name"));
  auto showAddress = [name, guard] {
    if(guard)
      name->setToolTip(LocalTree::scriptableAddress(*guard).toString());
  };
  showAddress();
  QObject::connect(name, &QLineEdit::editingFinished, widg, [name, guard, &ctx] {
    if(!guard)
      return;
    const auto& port = *guard;
    const auto txt = name->text();
    if(!txt.isEmpty() && txt != port.exposed())
      CommandDispatcher<>{ctx.commandStack}.submit<SetPortScriptingName>(port, txt);
  });
  QObject::connect(
      &port, &Port::exposedChanged, name, [name, showAddress](const QString& n) {
    if(n != name->text())
      name->setText(n);
    showAddress();
  });
  QObject::connect(&port, &Port::scriptableChanged, name, [showAddress](bool) {
    showAddress();
  });

  lay->addWidget(check);
  lay->addWidget(name);
  return widg;
}
}
