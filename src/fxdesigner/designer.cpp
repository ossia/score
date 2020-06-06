#include "widget.hpp"
#include "view.hpp"
#include "inspector.hpp"
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>

#include <core/application/MinimalApplication.hpp>
#include <score/plugins/PluginInstances.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>

#include <QJsonDocument>
#include <wobjectimpl.h>

#include <QInputDialog>
#include <QFileDialog>
#include <QProcess>
#include <QToolBar>

// TODO : implement remaining objects
// TODO set port pos in code
// TODO check port and text pos

namespace fxd
{
class DocumentPresenter final : public score::DocumentDelegatePresenter
{
  W_OBJECT(DocumentPresenter)
  friend class DisplayedElementsPresenter;

public:
  DocumentPresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const DocumentModel& delegate_model,
      DocumentView& delegate_view)
      : DocumentDelegatePresenter{parent_presenter,
                                  delegate_model,
                                  delegate_view}
      , m_ctx{ctx}
  {
  }

  ~DocumentPresenter() override {}

  void setNewSelection(const Selection& old, const Selection& s) override
  {
    for (auto& obj : m_curSel)
    {
      if (obj && !s.contains(obj))
      {
        auto c = obj->findChild<Selectable*>();
        if (c)
        {
          c->set_impl(false);
        }
      }
    }

    for (auto& obj : s)
    {
      if (obj)
      {
        auto c = obj->findChild<Selectable*>();
        if (c)
        {
          c->set_impl(true);
        }
      }
    }
    m_curSel = s;

    if(m_curSel.empty())
    {
      // "select" the document model
      m_ctx.selectionStack.pushNewSelection({&this->m_model});
    }
  }

private:

  const score::DocumentContext& m_ctx;
  Selection m_curSel;
};

class DocumentFactory final : public score::DocumentDelegateFactory
{
  SCORE_CONCRETE("bb7d624a-7e0d-41fa-b6ff-43f35b32c07d")

  score::DocumentDelegateView*
  makeView(const score::DocumentContext& ctx, QObject* parent) override
  {
    return new DocumentView{ctx, parent};
  }

  score::DocumentDelegatePresenter* makePresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view) override
  {
    return new DocumentPresenter{ctx,
                                 parent_presenter,
                                 static_cast<const DocumentModel&>(model),
                                 static_cast<DocumentView&>(view)};
  }

  void make(
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override
  {
    std::allocator<DocumentModel> alloc;
    auto res = alloc.allocate(1);
    ptr = res;
    new (&res) DocumentModel{ctx, parent};
  }

  void load(
      const VisitorVariant& vis,
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override
  {
    std::allocator<DocumentModel> alloc;
    auto res = alloc.allocate(1);
    ptr = res;
    score::deserialize_dyn(vis, [&](auto&& deserializer) {
      new (&res) DocumentModel{deserializer, ctx, parent};
      return ptr;
    });
  }
};

/// Code display
void showCode()
{
  auto doc = score::GUIAppContext().documents.currentDocument();
  if(!doc)
    return;

  auto& fx = fxModel(doc->context());

  QString code;
  int unnamed_count = 0;

  for(const Widget& widget : fx.widgets)
  {
    QString name = widget.name();
    if(name.isEmpty())
      name = QString("item%1").arg(unnamed_count++);

    code += widget.code(name) + "\n";
  }

  QInputDialog::getMultiLineText(nullptr, "code", "", code);
}
}
class score_addon_fxdesigner final
    : public score::Plugin_QtInterface,
      public score::FactoryInterface_QtInterface,
      public score::CommandFactory_QtInterface,
      public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "7adcc7af-7eb7-4486-bc91-28f57171e41c")

public:
  score_addon_fxdesigner() {}
  ~score_addon_fxdesigner() override {}

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override
  {
    return instantiate_factories<
        score::ApplicationContext,
        FW<score::DocumentDelegateFactory, fxd::DocumentFactory>
        , FW<Inspector::InspectorWidgetFactory, fxd::WidgetInspectorFactory, fxd::DocumentInspectorFactory>
        >(ctx, key);
  }

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override
  {
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
        fxd::CommandFactoryName(), CommandGeneratorMap{}};

    ossia::for_each_type<fxd::AddWidget>(
        score::commands::FactoryInserter{cmds.second});

    return cmds;
  }
};


W_OBJECT_IMPL(fxd::MoveHandle)
W_OBJECT_IMPL(fxd::ResizeHandle)
W_OBJECT_IMPL(fxd::DocumentModel)
W_OBJECT_IMPL(fxd::DocumentPresenter)

int main(int argc, char** argv)
{
  score_addon_fxdesigner addon;
  score::staticPlugins().push_back(&addon);

  QCoreApplication::setOrganizationName("OSSIA");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("FX Designer");

  QSettings s;
  s.setValue("PluginSettings/Whitelist",
             QStringList{"score_plugin_inspector"}
             );

  score::MinimalGUIApplication app{argc, argv};

  auto toolbar = app.view().findChildren<QToolBar*>(QString{}, Qt::FindDirectChildrenOnly).first();

  {
    auto act = new QAction(QObject::tr("Load code"));
    toolbar->addAction(act);
    QObject::connect(act, &QAction::triggered, [] {

      QFileDialog::getOpenFileContent("Header (*.hpp, *.h)", [] (const QString& name, const QByteArray& data) {

        fxd::DocumentModel& fxmodel = fxd::fxModel(score::GUIAppContext().currentDocument());

        // Extract metadata

        auto code = QString::fromUtf8(data);
        QString metadata_start = code.mid(code.indexOf("struct Metadata")).simplified();

        auto metadata = metadata_start.mid(0, metadata_start.indexOf(" };") + 4);

        // Read file used to parse
        QFile src{":/parser.cpp"};
        src.open(QIODevice::ReadOnly);
        QString str = src.readAll();

        str.replace("$$METADATA$$", metadata);
        {
          QFile dst{"/tmp/parser.cpp"};
          dst.open(QIODevice::WriteOnly);
          dst.write(str.toUtf8());
        }
        QProcess p;
        p.setWorkingDirectory("/tmp");
        p.setProgram("/usr/bin/clang++");
        p.setArguments(QStringList()
                       << "/tmp/parser.cpp"
                       << "-I/usr/include/qt"
                       << "-I/usr/include/qt/QtCore"
                       << "-fPIC"
                       << "-lQt5Core"
                       << "-std=c++2a"
                       );
        p.start();
        p.waitForFinished();
        if(p.exitCode() == 0)
        {
          p.setProgram("/tmp/a.out");
          p.start();
          p.waitForFinished();

          fxmodel.loadCode(QJsonDocument::fromJson(p.readAllStandardOutput()));
          //qDebug() << "ouptut : < " << QJsonDocument::fromJson(p.readAllStandardOutput());
          //qDebug() << "err : < " << p.readAllStandardError();
        }

        //fxmodel.loadCode(QString::fromUtf8(data));
      });
    });
  }

  {
    auto act = new QAction(QObject::tr("Print code"));
    toolbar->addAction(act);
    QObject::connect(act, &QAction::triggered, fxd::showCode);
  }

  // TODO move skin to lib..
  QFile f("./DefaultSkin.json");
  f.open(QIODevice::ReadOnly);
  auto skin = QJsonDocument::fromJson(f.readAll()).object();
  score::Skin::instance().load(skin);
  app.m_presenter->documentManager().newDocument(
      score::GUIAppContext(),
      Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
      *app.m_presenter->applicationComponents()
           .interfaces<score::DocumentDelegateList>()
           .begin());


  for(score::PanelDelegate& panel : app.context().panels())
  {
    if(panel.defaultPanelStatus().dock == Qt::RightDockWidgetArea)
    {
      panel.widget()->setFixedWidth(400);
    }
  }
  return app.exec();
}
