#pragma once
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/TimeValue.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QByteArray>
#include <QMimeData>

#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;

class SCORE_LIB_PROCESS_EXPORT ProcessDropHandler : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessDropHandler, "e0908e4a-9e88-4029-9a61-7658cc695152")
public:
  struct ProcessDrop
  {
    ProcessData creation;
    optional<TimeVal> duration;
    std::function<void(Process::ProcessModel&, score::Dispatcher&)> setup;
  };

  struct DroppedFile
  {
    QString filename;
    QByteArray data;
  };

  ProcessDropHandler();
  ~ProcessDropHandler() override;

  std::vector<ProcessDrop>
  getDrops(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;

  virtual QSet<QString> mimeTypes() const noexcept;
  virtual QSet<QString> fileExtensions() const noexcept;

protected:
  virtual std::vector<ProcessDrop>
  drop(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;

  virtual std::vector<ProcessDrop>
  dropData(const std::vector<DroppedFile>& data, const score::DocumentContext& ctx) const noexcept;
};

class SCORE_LIB_PROCESS_EXPORT ProcessDropHandlerList final
    : public score::InterfaceList<ProcessDropHandler>
{
public:
  ~ProcessDropHandlerList() override;

  std::vector<ProcessDropHandler::ProcessDrop>
  getDrop(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;

  static optional<TimeVal> getMaxDuration(const std::vector<ProcessDropHandler::ProcessDrop>&);
};
}
