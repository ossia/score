#include <Dataflow/Inspector/DataflowWidget.hpp>

namespace Dataflow
{

PortWidget::PortWidget(Explorer::DeviceExplorerModel& model, QWidget* parent)
  : QWidget{parent}
  , m_lay{this}
  , m_remove{tr("X"), this}
  , localName{this}
  , accessor{model, this}
{
  m_lay.addWidget(&localName);
  m_lay.addWidget(&accessor);
  m_lay.addWidget(&m_remove);

  con(m_remove, &QPushButton::clicked, this, &PortWidget::removeMe);
}

DataflowWidget::DataflowWidget(
    const score::DocumentContext& doc,
    const Process::DataflowProcess& proc,
    QWidget* parent)
  : QWidget{parent}
  , m_proc{proc}
  , m_explorer{doc.plugin<Explorer::DeviceDocumentPlugin>().explorer()}
  , m_disp{doc.commandStack}
  , m_lay{this}
{
  setLayout(&m_lay);

  reinit();
  /*
  con(proc, &Process::DataflowProcess::inletsChanged, this, &DataflowWidget::reinit, Qt::QueuedConnection);
  con(proc, &Process::DataflowProcess::outletsChanged, this, &DataflowWidget::reinit, Qt::QueuedConnection);
  */
}

void DataflowWidget::reinit()
{
  /*
  score::clearLayout(&m_lay);
  m_inlets.clear();
  m_outlets.clear();
  m_addInlet = nullptr;
  m_addOutlet = nullptr;

  m_lay.addWidget(new TextLabel{tr("Inlets"), this});

  std::size_t i{};
  for(auto& port : m_proc.inlets())
  {
    auto widg = new PortWidget{m_explorer, this};
    widg->localName.setText(port.customData);
    widg->accessor.setAddress(port.address);

    m_lay.addWidget(widg);
    m_inlets.push_back(widg);

    con(widg->accessor, &Explorer::AddressAccessorEditWidget::addressChanged,
        this, [=] (const Device::FullAddressAccessorSettings& as) {
      auto cur = m_proc.inlets()[i];
      cur.address = as.address;
      auto cmd = new EditPort{m_proc, std::move(cur), i, true};
      m_disp.submitCommand(cmd);
    }, Qt::QueuedConnection);

    con(widg->localName, &QLineEdit::editingFinished,
        this, [=] {
      auto cur = m_proc.inlets()[i];
      cur.customData = widg->localName.text();
      auto cmd = new EditPort{m_proc, std::move(cur), i, true};
      m_disp.submitCommand(cmd);
    }, Qt::QueuedConnection);

    connect(widg, &PortWidget::removeMe, this, [=] {
      m_disp.submitCommand<RemovePort>(m_proc, i, true);
    }, Qt::QueuedConnection);

    i++;
  }

  m_addInlet = new QPushButton{tr("Add inlet"), this};
  connect(m_addInlet, &QPushButton::clicked, this, [&] {
    m_disp.submitCommand<AddPort>(m_proc, true);
  }, Qt::QueuedConnection);
  m_lay.addWidget(m_addInlet);

  m_lay.addWidget(new TextLabel{tr("Outlets"), this});

  i = 0;
  for(auto& port : m_proc.outlets())
  {
    auto widg = new PortWidget{m_explorer, this};
    widg->localName.setText(port.customData);
    widg->accessor.setAddress(port.address);

    m_lay.addWidget(widg);
    m_outlets.push_back(widg);

    con(widg->accessor, &Explorer::AddressAccessorEditWidget::addressChanged,
        this, [=] (const Device::FullAddressAccessorSettings& as) {
      auto cur = m_proc.outlets()[i];
      cur.address = as.address;
      auto cmd = new EditPort{m_proc, std::move(cur), i, false};
      m_disp.submitCommand(cmd);
    }, Qt::QueuedConnection);

    con(widg->localName, &QLineEdit::editingFinished,
        this, [=] {
      auto cur = m_proc.outlets()[i];
      cur.customData = widg->localName.text();
      auto cmd = new EditPort{m_proc, std::move(cur), i, false};
      m_disp.submitCommand(cmd);
    }, Qt::QueuedConnection);

    connect(widg, &PortWidget::removeMe, this, [=] {
      m_disp.submitCommand<RemovePort>(m_proc, i, false);
    }, Qt::QueuedConnection);

    i++;
  }

  m_addOutlet = new QPushButton{tr("Add outlet"), this};
  connect(m_addOutlet, &QPushButton::clicked, this, [&] {
    m_disp.submitCommand<AddPort>(m_proc, false);
  }, Qt::QueuedConnection);
  m_lay.addWidget(m_addOutlet);
  */
}

}
