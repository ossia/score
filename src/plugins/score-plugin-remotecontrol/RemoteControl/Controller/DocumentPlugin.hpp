#pragma once

#include <Process/Process.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/flat_map.hpp>

#include <RemoteControl/RemoteControlProvider.hpp>
namespace RemoteControl::Controller
{
// In JS:
// connect to DocumentPlugin actions and map them to the JS actions.

// Multi-controllers : we want to map multiple fader pages
// DocumentPlugin combines all the inputs from all the controllers.

// Lock device & process to controller
class RemoteControlImpl;
class RemoteControlProvider;
class DocumentPlugin : public score::DocumentPlugin
{
public:
  using ControllerHandle = Process::RemoteControl::ControllerHandle;
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);
  ~DocumentPlugin();

  std::shared_ptr<Process::RemoteControl> acquireRemoteControlInterface();
  void releaseRemoteControlInterface(std::shared_ptr<Process::RemoteControl>);

  std::vector<ControllerHandle> registerControllerGroup(
      RemoteControlImpl&, Process::RemoteControl::ControllerHint hint, int count);

  void setControl(RemoteControlImpl&, ControllerHandle index, const ossia::value& val);
  void offsetControl(RemoteControlImpl&, ControllerHandle index, double val);

private:
  void on_selectionChanged(const Selection& old, const Selection& current);
  void deselectProcess();
  void selectProcess(Process::ProcessModel& cur);

  void on_controlChanged(
      Process::ProcessModel& p, Process::ControlInlet& inl, int idx,
      const ossia::value& v);

  struct Controller
  {
    std::shared_ptr<Process::RemoteControl> controller;
    std::vector<ControllerHandle> handles;
    struct ControlMap
    {
      ControllerHandle handle;
      QPointer<Process::ControlInlet> inlet;
    };

    std::vector<ControlMap> maps;
  };

  std::vector<Controller> m_controllers;
  JS::EditJsContext m_edit;

  ossia::flat_map<Process::ProcessModel*, std::vector<QMetaObject::Connection>>
      m_currentConnections;
};

}
