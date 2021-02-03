#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
namespace Process
{
PortListWidget::PortListWidget(
    const Process::ProcessModel& proc,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : QWidget{parent}, m_process{proc}, m_ctx{ctx}
{
  setLayout(new Inspector::Layout);
  reload();

  con(proc, &Process::ProcessModel::inletsChanged, this, [this] { reload(); });
  con(proc, &Process::ProcessModel::outletsChanged, this, [this] { reload(); });
}

void PortListWidget::reload()
{
  auto& old_lay = *(Inspector::Layout*)layout();
  auto& lay = *new Inspector::Layout;
  QWidget{}.setLayout(&old_lay);
  setLayout(&lay);

  auto& pf = m_ctx.app.interfaces<PortFactoryList>();

  if (!m_process.inlets().empty())
  {
    lay.addRow(tr("<b>Inputs</b>"), (QWidget*)nullptr);
    for (auto port : m_process.inlets())
    {
      auto fact = pf.get(port->concreteKey());
      if (fact)
      {
        fact->setupInletInspector(*port, m_ctx, this, lay, this);
      }
      else
      {
        PortWidgetSetup::setupAlone(*port, m_ctx, lay, this);
      }
    }
  }

  if (!m_process.outlets().empty())
  {
    lay.addRow(tr("<b>Outputs</b>"), (QWidget*)nullptr);
    for (auto port : m_process.outlets())
    {
      auto fact = pf.get(port->concreteKey());
      if (fact)
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

void PortWidgetSetup::setupAlone(
    const Port& port,
    const score::DocumentContext& ctx,
    Inspector::Layout& lay,
    QWidget* parent)
{
  setupImpl(port.customData(), port, ctx, lay, parent);
}

void PortWidgetSetup::setupInLayout(
    const Port& port,
    const score::DocumentContext& ctx,
    Inspector::Layout& lay,
    QWidget* parent)
{
  setupImpl(QObject::tr("Address"), port, ctx, lay, parent);
}

void PortWidgetSetup::setupControl(
    const ControlInlet& inlet,
    QWidget* inlet_widget,
    const score::DocumentContext& ctx,
    Inspector::Layout& vlay,
    QWidget* parent)
{
  auto widg = new QWidget;
  auto advBtn = new score::ArrowButton{Qt::RightArrow, widg};

  auto lab = new TextLabel{inlet.customData(), widg};
  auto hl = new score::MarginLess<QHBoxLayout>{widg};
  hl->addWidget(advBtn);
  hl->addWidget(lab);

  auto sw = new QWidget{parent};
  sw->setContentsMargins(0, 0, 0, 0);
  auto hl2 = new score::MarginLess<QHBoxLayout>{sw};
  hl2->addSpacing(30);
  auto lay = new Inspector::Layout{};
  Process::PortWidgetSetup::setupInLayout(inlet, ctx, *lay, sw);
  hl2->addLayout(lay);

  QObject::connect(advBtn, &QToolButton::clicked, sw, [=] {
    sw->setVisible(!sw->isVisible());
    advBtn->setArrowType(advBtn->arrowType() == Qt::RightArrow ? Qt::DownArrow : Qt::RightArrow);
  });
  sw->setVisible(false);

  vlay.addRow(widg, inlet_widget);
  vlay.addRow(sw);
}

void PortWidgetSetup::setupControl(
    const ControlOutlet& inlet,
    QWidget* inlet_widget,
    const score::DocumentContext& ctx,
    Inspector::Layout& vlay,
    QWidget* parent)
{
  auto widg = new QWidget;
  auto advBtn = new score::ArrowButton{Qt::RightArrow, widg};

  auto lab = new TextLabel{inlet.customData(), widg};
  auto hl = new score::MarginLess<QHBoxLayout>{widg};
  hl->addWidget(advBtn);
  hl->addWidget(lab);

  auto sw = new QWidget{parent};
  sw->setContentsMargins(0, 0, 0, 0);
  auto hl2 = new score::MarginLess<QHBoxLayout>{sw};
  hl2->addSpacing(30);
  auto lay = new Inspector::Layout{};
  Process::PortWidgetSetup::setupInLayout(inlet, ctx, *lay, sw);
  hl2->addLayout(lay);

  QObject::connect(advBtn, &QToolButton::clicked, sw, [=] {
    sw->setVisible(!sw->isVisible());
    advBtn->setArrowType(advBtn->arrowType() == Qt::RightArrow ? Qt::DownArrow : Qt::RightArrow);
  });
  sw->setVisible(false);

  vlay.addRow(widg, inlet_widget);
  vlay.addRow(sw);
}

QWidget* PortWidgetSetup::makeAddressWidget(
    const Port& port,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  using namespace Device;
  auto edit = new AddressAccessorEditWidget{ctx, parent};
  edit->setAddress(port.address());

  QObject::connect(&port, &Port::addressChanged, edit, [edit](const State::AddressAccessor& addr) {
    if (addr != edit->address().address)
    {
      edit->setAddress(addr);
    }
  });

  QObject::connect(
      edit,
      &AddressAccessorEditWidget::addressChanged,
      parent,
      [&port, &ctx](const auto& newAddr) {
        if (newAddr.address == port.address())
          return;

        CommandDispatcher<>{ctx.dispatcher}.submit(
            new Process::ChangePortAddress{port, newAddr.address});
      });

  return edit;
}

void PortWidgetSetup::setupImpl(
    const QString& txt,
    const Port& port,
    const score::DocumentContext& ctx,
    Inspector::Layout& lay,
    QWidget* parent)
{
  auto widg = new QWidget;
  auto hl = new score::MarginLess<QHBoxLayout>{widg};

  auto advBtn = new QToolButton{widg};
  advBtn->setIconSize(QSize{16, 16});
  hl->addWidget(advBtn);

  auto lab = new TextLabel{txt, widg};
  hl->addWidget(lab);

  auto port_widg = PortWidgetSetup::makeAddressWidget(port, ctx, parent);
  lay.addRow(widg, port_widg);

  switch (port.type())
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
  }
}
}
