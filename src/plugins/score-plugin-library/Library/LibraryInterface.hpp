#pragma once
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/std/StringHash.hpp>
#include <score_plugin_library_export.h>
class QAbstractItemModel;
class QMimeData;

namespace Library
{
class ProcessesItemModel;
class FileSystemModel;
class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterface
    : public score::InterfaceBase
{
  SCORE_INTERFACE(LibraryInterface, "9b94d974-9f2d-4986-a62b-b69e51a4d305")
public:
  ~LibraryInterface() override;

  virtual QSet<QString> acceptedFiles() const noexcept;
  virtual QSet<QString> acceptedMimeTypes() const noexcept;

  virtual QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept;

  virtual void
  setup(ProcessesItemModel& model, const score::GUIApplicationContext& ctx);
  virtual bool onDrop(
      FileSystemModel& model,
      const QMimeData& mime,
      int row,
      int column,
      const QModelIndex& parent);

  virtual bool
  onDoubleClick(const QString& path, const score::DocumentContext& ctx);
};

class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterfaceList final
    : public score::InterfaceList<LibraryInterface>
{
public:
  ~LibraryInterfaceList() override;
};

class LibraryDocumentLoader final
    : public LibraryInterface
{
  SCORE_CONCRETE("e4785238-af94-4fe9-9e5b-12b9555a2482")
public:
  ~LibraryDocumentLoader() override;

  QSet<QString> acceptedFiles() const noexcept override;

  bool
  onDoubleClick(const QString& path, const score::DocumentContext& ctx) override;
};
}
