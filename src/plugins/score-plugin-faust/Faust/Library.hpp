#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Faust/EffectModel.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <QFileInfo>
#include <QTimer>
#include <QDirIterator>

#include <unordered_map>

namespace Faust
{
class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("e274ee7b-9142-43a0-9d77-9286a63af4d9")

  QSet<QString> acceptedFiles() const noexcept override { return {"dsp"}; }

  static inline const QRegularExpression nameExpr{R"_(declare name "([a-zA-Z0-9_-]+)";)_"};
  static inline const QRegularExpression authorExpr{R"_(declare author "([a-zA-Z0-9_-]+)";)_"};
  static inline const QRegularExpression descExpr{R"_(declare description "([a-zA-Z0-9.<>\(\):/~, _-]+)";)_"};

  QDir libraryFolder;
  QDirIterator iterator{QString{}};
  Library::ProcessNode* parent{};
  std::unordered_map<QString, Library::ProcessNode*> categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx) override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = FaustEffectFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
      return;

    parent = reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    // We use the parent folder as category...
    libraryFolder.setPath(ctx.settings<Library::Settings::Model>().getPath());

    iterator.~QDirIterator();
    new (&iterator) QDirIterator{
      libraryFolder.absolutePath(),
      {"*.dsp"},
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
    pdata.key = Metadata<ConcreteKey_k, FaustEffectModel>::get();
    pdata.customData = [&] {
      return file.absoluteFilePath();
    }();
    pdata.author = "Faust standard library";

    {
      auto matches = nameExpr.match(pdata.customData);
      if(matches.hasMatch())
      {
        pdata.prettyName = matches.captured(1);
      }
    }
    {
      auto matches = authorExpr.match(pdata.customData);
      if(matches.hasMatch())
      {
        pdata.author = matches.captured(1);
      }
    }
    {
      auto matches = descExpr.match(pdata.customData);
      if(matches.hasMatch())
      {
        pdata.description = matches.captured(1);
      }
    }

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
  SCORE_CONCRETE("1e83a000-5aca-4427-8de5-1dc7a390e201")

  QSet<QString> fileExtensions() const noexcept override { return {"dsp"}; }

  std::vector<Process::ProcessDropHandler::ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

    for (auto&& [filename, _] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Faust::FaustEffectModel>::get();
      // TODO use faust-provided name
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.creation.customData = std::move(filename);

      vec.push_back(std::move(p));
    }

    return vec;
  }
};

}
