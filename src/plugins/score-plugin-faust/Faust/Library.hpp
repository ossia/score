#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <QFileInfo>
#include <QTimer>

#include <Faust/EffectModel.hpp>

#include <unordered_map>

namespace Faust
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("e274ee7b-9142-43a0-9d77-9286a63af4d9")

  QSet<QString> acceptedFiles() const noexcept override { return {"dsp"}; }

  static inline const QRegularExpression nameExpr{
      R"_(declare name "([a-zA-Z0-9_\-]+)";)_"};
  static inline const QRegularExpression authorExpr{
      R"_(declare author "([a-zA-Z0-9_\-]+)";)_"};
  static inline const QRegularExpression descExpr{
      R"_(declare description "([a-zA-Z0-9.<>\(\):/~, _\-]+)";)_"};

  Library::Subcategories categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = FaustEffectFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    categories.init(node, ctx);
  }

  void addPath(std::string_view path) override
  {
    score::PathInfo file{path};
    if(file.fileName != "layout.dsp")
      registerDSP(file);
  }

  void registerDSP(const score::PathInfo& file)
  {
    Library::ProcessData pdata;
    pdata.prettyName
        = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
    pdata.key = Metadata<ConcreteKey_k, FaustEffectModel>::get();
    pdata.customData
        = QString::fromUtf8(file.absoluteFilePath.data(), file.absoluteFilePath.size());
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

    categories.add(file, std::move(pdata));
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("1e83a000-5aca-4427-8de5-1dc7a390e201")

  QSet<QString> fileExtensions() const noexcept override { return {"dsp"}; }

  void dropPath(
      std::vector<ProcessDrop>& vec, const QString& filename,
      const score::DocumentContext& ctx) const noexcept override
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Faust::FaustEffectModel>::get();

    // TODO use faust-provided name
    p.creation.prettyName = QFileInfo{filename}.baseName();
    p.creation.customData = std::move(filename);

    vec.push_back(std::move(p));
  }
};

}
