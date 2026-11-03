#pragma once
#include <Process/Dataflow/PortType.hpp>

#include <QPointer>
#include <QString>

#include <score_plugin_dataflow_export.h>

#include <vector>

namespace score
{
struct DocumentContext;
}
namespace Process
{
class Cable;
class Port;
class ProcessModel;
}
namespace Scenario
{
class ScenarioDocumentModel;
}

namespace Dataflow
{
//! One connection of a document: a cable between two ports, or a port bound
//! to a device address.
struct SCORE_PLUGIN_DATAFLOW_EXPORT ConnectionRow
{
  enum Kind : uint8_t
  {
    Cable,
    Address
  };
  //! Where an address binding stands
  enum Status : uint8_t
  {
    Resolved,
    MissingDevice,
    MissingNode,
    Pattern
  };

  Kind kind{};
  Status status{Resolved};
  //! For an address binding, true when the address feeds the port
  bool addressIsSource{};
  Process::PortType portType{};

  QPointer<Process::Cable> cable;
  //! The cable's source, or the bound port
  QPointer<Process::Port> sourcePort;
  //! The cable's sink; null for an address binding
  QPointer<Process::Port> sinkPort;
  QPointer<Process::ProcessModel> sourceProcess;
  QPointer<Process::ProcessModel> sinkProcess;

  QString from;
  QString to;
  //! Process name, or device name for the address end
  QString fromGroup;
  QString toGroup;
  QString device;
  QString address;
  QString mode;
  QString location;
};

SCORE_PLUGIN_DATAFLOW_EXPORT
QString portTypeName(Process::PortType t);
SCORE_PLUGIN_DATAFLOW_EXPORT
QString cableTypeName(int t);
SCORE_PLUGIN_DATAFLOW_EXPORT
QString statusName(ConnectionRow::Status s);

//! "Process: port", with the port's index when it has no name
SCORE_PLUGIN_DATAFLOW_EXPORT
QString describePort(const Process::Port& port);

//! Names of the intervals and processes above obj, outermost first
SCORE_PLUGIN_DATAFLOW_EXPORT
QString hierarchyPath(const QObject& obj);

//! Every cable of the document, then every port bound to an address
SCORE_PLUGIN_DATAFLOW_EXPORT
std::vector<ConnectionRow> collectConnections(
    const Scenario::ScenarioDocumentModel& doc, const score::DocumentContext& ctx);
}
