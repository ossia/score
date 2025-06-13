#include "EditContext.hpp"

#include <Device/Protocol/ProtocolList.hpp>

#include <Process/ProcessList.hpp>

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
}
