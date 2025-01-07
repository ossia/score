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

    if(!panel)
      panel = &score::GUIAppContext().panel<JS::PanelDelegate>();

    panel->importModule(path);
    return true;
  }

  void addPath(std::string_view path) override
  {
    if(path.find("companion-bundled-modules") == std::string_view::npos)
      add(QString::fromUtf8(path.data(), path.length()));
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

  static inline const QRegularExpression scoreImport{"import Score [0-9].[0-9]"};

  Library::Subcategories categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, JS::ProcessModel>::get();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    categories.init(node, ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, JS::ProcessModel>::get();
    pdata.customData = [&] {
      QFile f(file.absoluteFilePath());
      f.open(QIODevice::ReadOnly);
      return f.readAll().trimmed();
    }();

    {
      auto matches = scoreImport.match(pdata.customData);
      if(matches.hasMatch())
      {
        categories.add(file, std::move(pdata));
      }
    }
  }
};

}
