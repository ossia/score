#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Pd/PdProcess.hpp>

#include <QFileInfo>
#include <QTimer>

namespace Pd
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("01ffc109-9cb3-4c5a-9cdd-d9fd38fe5e17")

  QSet<QString> acceptedFiles() const noexcept override { return {"pd"}; }

  Library::Subcategories categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, Pd::ProcessModel>::get();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    categories.init(
        Metadata<PrettyName_k, Pd::ProcessModel>::get().toStdString(), node, ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, Pd::ProcessModel>::get();
    pdata.customData = [&] { return file.absoluteFilePath(); }();

    categories.add(file, std::move(pdata));
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("3c4379ce-a5b9-456f-b1d6-09f3f02cc67e")

  QSet<QString> fileExtensions() const noexcept override { return {"pd"}; }

  void dropPath(
      std::vector<ProcessDrop>& vec, const score::FilePath& filename,
      const score::DocumentContext& ctx) const noexcept override
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Pd::ProcessModel>::get();
    p.creation.prettyName = filename.basename;
    p.creation.customData = filename.relative;

    vec.push_back(std::move(p));
  }
};

}
