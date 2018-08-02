#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Commands/EditPort.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/document/DocumentContext.hpp>
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <QFormLayout>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
namespace Process
{
PortListWidget::PortListWidget(
    const Process::ProcessModel& proc
    , const score::DocumentContext& ctx
    , QWidget* parent)
  : QWidget{parent}
  , m_process{proc}
  , m_ctx{ctx}
{
  setLayout(new QFormLayout);
  reload();

  con(proc, &Process::ProcessModel::inletsChanged, this, [this] { reload(); });
  con(proc, &Process::ProcessModel::outletsChanged, this, [this] { reload(); });
}

void PortListWidget::reload()
{
  using namespace Device;
  auto& lay = *(QFormLayout*)layout();
  score::clearLayout(&lay);

  auto setup = [&] (Process::Port* port) {
    auto edit = new Device::AddressAccessorEditWidget{m_ctx, this};
    edit->setAddress(port->address());

    connect(port, &Port::addressChanged, edit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        edit, &AddressAccessorEditWidget::addressChanged, this,
          [=] (const auto& newAddr) {
      if (newAddr.address == port->address())
        return;

      if (newAddr.address.address.path.isEmpty())
        return;

      CommandDispatcher<>{m_ctx.dispatcher}.submitCommand(new Process::ChangePortAddress{*port, newAddr.address});
    });

    QString str;
    switch(port->type)
    {
      case Process::PortType::Audio:
        str += QString::fromUtf8("<b>〜</b> ");
        break;
      case Process::PortType::Midi:
        str += QString::fromUtf8("<b>♪</b> ");
        break;
      case Process::PortType::Message:
        str += QString::fromUtf8("<b>⇢</b> ");
        break;
    }
    str += port->customData();
    lay.addRow(str, edit);
  };

  if(!m_process.inlets().empty())
  {
    lay.addRow(tr("<b>Inputs</b>"), (QWidget*)nullptr);
    for(auto port : m_process.inlets())
    {
      setup(port);
    }
  }

  if(!m_process.outlets().empty())
  {
    lay.addRow(tr("<b>Outputs</b>"), (QWidget*)nullptr);
    for(auto port : m_process.outlets())
    {
      setup(port);
    }
  }
}
}

