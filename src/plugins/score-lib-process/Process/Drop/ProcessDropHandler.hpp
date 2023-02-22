#pragma once
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/TimeValue.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/FilePath.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QByteArray>
#include <QMimeData>

#include <score_lib_process_export.h>

#include <string>

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
    std::optional<TimeVal> duration;
    std::function<void(Process::ProcessModel&, score::Dispatcher&)> setup;
  };

  struct DroppedFile
  {
    score::FilePath filename;
    QByteArray data;
  };

  ProcessDropHandler();
  ~ProcessDropHandler() override;

  void getCustomDrops(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept;

  void getMimeDrops(
      std::vector<ProcessDrop>& drops, const QMimeData& mime, const QString& fmt,
      const score::DocumentContext& ctx) const noexcept;

  void getFileDrops(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::FilePath& path, const score::DocumentContext& ctx) const noexcept;

  virtual QSet<QString> mimeTypes() const noexcept;
  virtual QSet<QString> fileExtensions() const noexcept;

protected:
  virtual void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept;

  virtual void dropPath(
      std::vector<ProcessDrop>& drops, const score::FilePath& path,
      const score::DocumentContext& ctx) const noexcept;

  virtual void dropData(
      std::vector<ProcessDrop>& drops, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept;
};

class SCORE_LIB_PROCESS_EXPORT ProcessDropHandlerList final
    : public score::InterfaceList<ProcessDropHandler>
{
public:
  ~ProcessDropHandlerList() override;

  std::vector<ProcessDropHandler::ProcessDrop>
  getDrop(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept;

  static std::optional<TimeVal>
  getMaxDuration(const std::vector<ProcessDropHandler::ProcessDrop>&);

private:
  void initCaches() const;
  mutable ossia::hash_map<std::string, ProcessDropHandler*> m_perMimeTypes{};
  mutable ossia::hash_map<std::string, ProcessDropHandler*> m_perFileExtension{};
  mutable std::size_t m_lastCacheSize{};
};
}
