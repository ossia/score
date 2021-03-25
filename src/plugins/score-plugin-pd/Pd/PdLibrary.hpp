#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Pd/PdProcess.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <QFileInfo>
#include <QTimer>
#include <QDirIterator>

#include <unordered_map>

namespace Pd
{
class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("01ffc109-9cb3-4c5a-9cdd-d9fd38fe5e17")

  QSet<QString> acceptedFiles() const noexcept override { return {"pd"}; }

  QDir libraryFolder;
  QDirIterator iterator{QString{}};
  Library::ProcessNode* parent{};
  std::unordered_map<QString, Library::ProcessNode*> categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx) override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, ProcessModel>::get();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
      return;

    parent = reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    // We use the parent folder as category...
    libraryFolder.setPath(ctx.settings<Library::Settings::Model>().getPath());

    iterator.~QDirIterator();
    new (&iterator) QDirIterator{
      libraryFolder.absolutePath(),
      {"*.pd"},
      QDir::NoFilter,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
    };

    next();
  }

  void next()
  {
    if (iterator.hasNext())
    {
      if(auto file = QFileInfo{iterator.next()}; file.fileName() != "layout.dsp")
        registerDSP(file);
      QTimer::singleShot(1, this, &LibraryHandler::next);
    }
  }

  void registerDSP(const QFileInfo& file)
  {
    Library::ProcessData pdata;
    pdata.prettyName = file.baseName();
    pdata.key = Metadata<ConcreteKey_k, Pd::ProcessModel>::get();
    pdata.customData = [&] {
      return file.absoluteFilePath();
    }();

    auto parentFolder = file.dir().dirName();
    if(auto it = categories.find(parentFolder); it != categories.end())
    {
      it->second->emplace_back(std::move(pdata), it->second);
    }
    else
    {
      if(file.dir() == libraryFolder)
      {
        parent->emplace_back(std::move(pdata), parent);
      }
      else
      {
        auto& category = parent->emplace_back(Library::ProcessData{{{}, parentFolder, {}}, {}, {}, {}}, parent);
        category.emplace_back(std::move(pdata), &category);
        categories[parentFolder] = &category;
      }
    }
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("3c4379ce-a5b9-456f-b1d6-09f3f02cc67e")

  QSet<QString> fileExtensions() const noexcept override { return {"pd"}; }

  std::vector<Process::ProcessDropHandler::ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

    for (auto&& [filename, _] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Pd::ProcessModel>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.creation.customData = std::move(filename);

      vec.push_back(std::move(p));
    }

    return vec;
  }
};

}
