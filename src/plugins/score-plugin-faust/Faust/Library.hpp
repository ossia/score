#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/application/ApplicationServices.hpp>

#include <QFile>
#include <QGuiApplication>
#include <QTimer>

#include <Faust/Descriptor.hpp>
#include <Faust/EffectModel.hpp>
namespace Faust
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("e274ee7b-9142-43a0-9d77-9286a63af4d9")

  QSet<QString> acceptedFiles() const noexcept override { return {"dsp"}; }

  Library::CategoryPaths categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    categories.init(
        Metadata<PrettyName_k, Faust::FaustEffectModel>::get().toStdString(), ctx);
  }

  std::optional<Library::ProcessEntry> scanPath(std::string_view path) override
  {
    score::PathInfo file{path};
    if(file.fileName == "layout.dsp")
      return std::nullopt;

    Library::ProcessData pdata;
    pdata.prettyName = QString::fromUtf8(
        file.completeBaseName.data(), file.completeBaseName.size());
    pdata.key = Metadata<ConcreteKey_k, FaustEffectModel>::get();
    pdata.customData = QString::fromUtf8(
        file.absoluteFilePath.data(), file.absoluteFilePath.size());

    // FIXME:
    /*
      auto desc = initDescriptor(pdata.customData);
      if(!desc.prettyName.isEmpty())
        pdata.prettyName = desc.prettyName;
    */

    return Library::ProcessEntry{
        pdata.key, categories(file), {std::move(pdata), {}}};
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("1e83a000-5aca-4427-8de5-1dc7a390e201")

  QSet<QString> fileExtensions() const noexcept override { return {"dsp"}; }

  void dropPath(
      std::vector<ProcessDrop>& vec, const score::FilePath& filename,
      const score::DocumentContext& ctx) const noexcept override
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Faust::FaustEffectModel>::get();

    // TODO use faust-provided name
    p.creation.prettyName = filename.basename;
    p.creation.customData = filename.relative;

    vec.push_back(std::move(p));
  }
};

}
