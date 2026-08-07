#pragma once
#include <JS/ConsolePanel.hpp>
#include <JS/JSProcessModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <QFile>
#include <QFileInfo>
namespace JS
{
class ModuleLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("6e72e377-efdd-4e3c-9900-922b618e7d70")

public:
  JS::PanelDelegate* panel{};

  QSet<QString> acceptedFiles() const noexcept override { return {"mjs"}; }

  bool add(const QString& path)
  {
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return false;

    // there is no panel without a GUI
    if(!panel)
      panel = score::GUIAppContext().findPanel<JS::PanelDelegate>();
    if(!panel)
      return false;

    panel->importModule(path);
    return true;
  }

  std::function<void()> asyncAddPath(std::string_view path) override
  {
    if(path.find("companion-bundled-modules") != std::string_view::npos)
      return {};
    if(path.find("node_modules") != std::string_view::npos)
      return {};

    // Everything else must happen on GUI thread (QML engine interaction)
    QString qpath = QString::fromUtf8(path.data(), path.length());
    return [this, qpath = std::move(qpath)]() {
      add(qpath);
    };
  }

  bool onDoubleClick(const QString& path, const score::DocumentContext& ctx) override
  {
    return add(path);
  }
};

class ConsoleLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("21f405da-a249-4e39-b405-9173aff11b26")

  QSet<QString> acceptedFiles() const noexcept override { return {"js"}; }

  bool onDoubleClick(const QString& path, const score::DocumentContext& ctx) override
  {
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return false;

    auto& p = ctx.app.panel<JS::PanelDelegate>();
    if(QFileInfo{f}.suffix() == "mjs")
    {
      p.importModule(path);
    }
    else
    {
      auto data = f.readAll();
      p.evaluate(data);
    }
    return true;
  }
};

class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("5231ea8b-da66-4c6f-9e34-d9a79cbc494a")

  QSet<QString> acceptedFiles() const noexcept override { return {"qml"}; }

  static inline const QRegularExpression scoreImport{"import Score"};

  Library::CategoryPaths categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    categories.init(
        Metadata<PrettyName_k, JS::ProcessModel>::get().toStdString(), ctx);
  }

  std::optional<Library::ProcessEntry> scanPath(std::string_view path) override
  {
    if(std::string_view{path}.ends_with(".ui.qml"))
      return std::nullopt;

    score::PathInfo pathinfo{path};
    QFile file{QString::fromUtf8(
        pathinfo.absoluteFilePath.data(), pathinfo.absoluteFilePath.size())};
    if(!file.open(QIODevice::ReadOnly))
      return std::nullopt;

    auto data = file.readAll().trimmed();
    auto matches = scoreImport.match(data);
    if(!matches.hasMatch())
      return std::nullopt;

    Library::ProcessData pdata;
    pdata.prettyName = QString::fromUtf8(
        pathinfo.completeBaseName.data(), pathinfo.completeBaseName.size());
    pdata.key = Metadata<ConcreteKey_k, JS::ProcessModel>::get();
    pdata.customData = QString::fromUtf8(
        pathinfo.absoluteFilePath.data(), pathinfo.absoluteFilePath.size());

    return Library::ProcessEntry{
        pdata.key, categories(pathinfo), {std::move(pdata), {}}};
  }
};

}
