#include "EditContext.hpp"

#include <Device/Protocol/ProtocolList.hpp>

#include <Process/ProcessList.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProjectLibraryWidget.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/Presenter.hpp>

#include <QGuiApplication>
namespace JS
{
QVariant EditJsContext::availableProcesses() const noexcept
{
  QVariantMap v;
  auto& ctx = score::AppContext();
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  for(const Process::ProcessModelFactory& proc : procs)
  {
    auto uid = score::uuids::toByteArray(proc.concreteKey().impl());
    auto desc = proc.descriptor("");
    v[uid] = QVariantMap{
        {"Name", proc.prettyName()},
        {"Category", proc.category()},
        {"Description", desc.description},
        {"Author", desc.author},
        {"Documentation", desc.documentationLink},
    };
  }
  return v;
}
QVariant EditJsContext::availableProcessesAndPresets() const noexcept
{
  QVariantList v;
  v.reserve(20000);
  auto& ctx = score::GUIAppContext();
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  auto& lib = ctx.panel<Library::ProcessPanel>().processWidget().processModel();
  auto& root = lib.rootNode();
  root.visit([&](const Library::ProcessNode& n) {
    if(n.key.impl().is_nil())
    {
      return;
    }
    auto uid = score::uuids::toByteArray(n.key.impl());
    auto& proc = *procs.get(n.key);
    // auto desc = proc.descriptor("");
    v.push_back(
        QVariantMap{
            {"ProcessName", proc.prettyName()},
            {"Name", n.prettyName},
            {"Key", QString::fromLatin1(uid)},
            {"CustomData", n.customData}});
  });

  return v;
}

QVariant EditJsContext::availableProtocols() const noexcept
{
  QVariantMap v;
  auto& ctx = score::AppContext();
  auto& procs = ctx.interfaces<Device::ProtocolFactoryList>();
  for(const Device::ProtocolFactory& proc : procs)
  {
    auto uid = score::uuids::toByteArray(proc.concreteKey().impl());
    v[uid] = QVariantMap{
        {"Name", proc.prettyName()},
        {"Category", proc.category()},
        {"Documentation", proc.manual()}};
  }
  return v;
}

QByteArray EditJsContext::serializeAsJson() noexcept
{
  auto doc = (score::Document*)document();
  if(!doc)
    return {};
  JSONReader w;
  w.buffer.Reserve(1024 * 1024 * 16);

  doc->saveAsJson(w);
  return w.toByteArray();
}
}
