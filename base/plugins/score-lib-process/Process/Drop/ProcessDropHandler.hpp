#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/command/AggregateCommand.hpp>

#include <QByteArray>
#include <QStringList>
#include <QMimeData>

#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;
struct Dispatcher
{
  virtual ~Dispatcher();
  virtual void submit(score::Command*) = 0;
};

template<typename T>
struct Dispatcher_T final : Dispatcher
{
  explicit Dispatcher_T(T& t): impl{t} { }
  T& impl;
  void submit(score::Command* c) { impl.submit(c); }
};

class SCORE_LIB_PROCESS_EXPORT ProcessDropHandler
    : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessDropHandler, "e0908e4a-9e88-4029-9a61-7658cc695152")
public:
  ~ProcessDropHandler() override;

  struct ProcessDrop
  {
    ProcessData creation;
    optional<TimeVal> duration;
    std::function<void(Process::ProcessModel&, Dispatcher&)> setup;
  };

public:
  std::vector<ProcessDrop> getDrops(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;

protected:
  virtual QSet<QString> mimeTypes() const noexcept;
  virtual QSet<QString> fileExtensions() const noexcept;
  virtual std::vector<ProcessDrop> drop(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;
  virtual std::vector<ProcessDrop> drop(const std::vector<QByteArray>& data, const score::DocumentContext& ctx) const noexcept;
};

class ProcessDropHandlerList final
    : public score::InterfaceList<ProcessDropHandler>
{
public:
  ~ProcessDropHandlerList() override;

   std::vector<ProcessDropHandler::ProcessDrop> getDrop(
       const QMimeData& mime
       , const score::DocumentContext& ctx) const noexcept;

   static TimeVal getMaxDuration(const std::vector<ProcessDropHandler::ProcessDrop>&) noexcept;
};
}
