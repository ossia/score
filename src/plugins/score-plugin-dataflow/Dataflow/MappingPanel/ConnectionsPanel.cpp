#include "ConnectionsPanel.hpp"

#include <Dataflow/MappingPanel/ConnectionsWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/selection/Selection.hpp>
#include <score/widgets/HelpInteraction.hpp>

namespace Dataflow
{
ConnectionsPanel::ConnectionsPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new ConnectionsWidget}
{
  m_widget->setObjectName("ConnectionsPanel");
  score::setHelp(
      m_widget,
      QObject::tr("The cables of the document and the ports bound to addresses."));
}

ConnectionsPanel::~ConnectionsPanel() = default;

QWidget* ConnectionsPanel::widget()
{
  return m_widget;
}

const score::PanelStatus& ConnectionsPanel::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::LeftDockWidgetArea,
      5,
      QObject::tr("Connections"),
      "connections",
      QObject::tr("Ctrl+Shift+K")};

  return status;
}

void ConnectionsPanel::on_modelChanged(score::MaybeDocument, score::MaybeDocument newm)
{
  m_widget->setDocument(newm);
}

void ConnectionsPanel::setNewSelection(const Selection& s)
{
  m_widget->setSelection(s);
}

std::unique_ptr<score::PanelDelegate>
ConnectionsPanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<ConnectionsPanel>(ctx);
}
}
