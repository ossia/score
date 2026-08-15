#pragma once
#include <Library/ProcessEntry.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/std/StringHash.hpp>

#include <score_plugin_library_export.h>

#include <functional>
class QAbstractItemModel;
class QMimeData;
class QDir;
namespace Process
{
struct Preset;
}

namespace Library
{
class ProcessesItemModel;
class FileSystemModel;
class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterface : public score::InterfaceBase
{
  SCORE_INTERFACE(LibraryInterface, "9b94d974-9f2d-4986-a62b-b69e51a4d305")
public:
  ~LibraryInterface() override;

  virtual QSet<QString> acceptedFiles() const noexcept;
  virtual QSet<QString> acceptedMimeTypes() const noexcept;

  virtual QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept;
  virtual QWidget*
  previewWidget(const Process::Preset& preset, QWidget* parent) const noexcept;

  virtual void setup(ProcessesItemModel& model, const score::GUIApplicationContext& ctx);
  virtual void addPath(std::string_view);
  virtual void removePath(std::string_view);

  /// Called on a worker thread during async scanning. Pure: no model access,
  /// no GUI objects — return the staged entry for this file, or nullopt to
  /// reject it. The model publishes accepted entries on the GUI thread with
  /// correct insertion signals. This is how process-library scan handlers
  /// contribute files; handlers that commit to something other than the
  /// process tree use asyncAddPath instead.
  virtual std::optional<ProcessEntry> scanPath(std::string_view path);

  /// Called on a worker thread during async scanning.
  /// Return a non-empty function to accept the file; that function
  /// will be invoked on the GUI thread (the "commit" phase).
  /// Only for handlers whose commit does not touch the ProcessesItemModel
  /// tree (e.g. the QML module panel, the preset library): tree insertions
  /// must go through scanPath so they are published with correct signals.
  /// Default implementation defers everything to addPath on the GUI thread.
  virtual std::function<void()> asyncAddPath(std::string_view path);
  virtual bool onDrop(const QMimeData& mime, int row, int column, const QDir& parent);

  virtual bool onDoubleClick(const QString& path, const score::DocumentContext& ctx);
};

class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterfaceList final
    : public score::InterfaceList<LibraryInterface>
{
public:
  ~LibraryInterfaceList() override;
};

class LibraryDocumentLoader final : public LibraryInterface
{
  SCORE_CONCRETE("e4785238-af94-4fe9-9e5b-12b9555a2482")
public:
  ~LibraryDocumentLoader() override;

  QSet<QString> acceptedFiles() const noexcept override;

  bool onDoubleClick(const QString& path, const score::DocumentContext& ctx) override;
};
}
