#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <QFileInfo>
#include <QTimer>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternFactory.hpp>
#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternParsing.hpp>

namespace Patternist
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("81375b6a-2172-49e5-bbf9-0d2eecc30677")

  QSet<QString> acceptedFiles() const noexcept override { return {"pat"}; }

  Library::Subcategories categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, Patternist::ProcessModel>::get();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    categories.init(
        Metadata<PrettyName_k, Patternist::ProcessModel>::get().toStdString(), node,
        ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, Patternist::ProcessModel>::get();
    pdata.customData = file.absoluteFilePath();
    categories.add(file, std::move(pdata));
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("6ba4246f-45cd-49f5-a103-8718ec52576b")

  QSet<QString> fileExtensions() const noexcept override { return {"pat"}; }

  void dropData(
      std::vector<ProcessDrop>& vec, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    const auto& [filename, content] = data;

    auto pat = parsePatterns(content);
    if(!pat.empty())
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Patternist::ProcessModel>::get();
      p.creation.prettyName = filename.basename;
      p.setup = [pat = std::move(pat)](
                    Process::ProcessModel& m, score::Dispatcher& disp) mutable {
        auto& proc = static_cast<Patternist::ProcessModel&>(m);
        disp.submit(new Patternist::UpdatePatterns{proc, std::move(pat)});
      };
      vec.push_back(std::move(p));
    }
  }
};

}
