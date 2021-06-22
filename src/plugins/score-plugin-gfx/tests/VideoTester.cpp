#include "../../../../tests/Integration/score_integration.hpp"
#include <verdigris>

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QMimeData>
#include <wobjectimpl.h>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#define private public
#include <Engine/ApplicationPlugin.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.cpp>
#include <Scenario/Commands/Cohesion/CreateCurves.cpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
void VideoTest()
{
  const auto& ctx = score::GUIAppContext();

  auto doc = ctx.docManager.newDocument(
      ctx,
      Id<score::DocumentModel>(0),
      *ctx.interfaces<score::DocumentDelegateList>().begin());

  qApp->processEvents();
  auto& doc_pm = doc->model().modelDelegate();

  // Load a video process
  // Create a window device
  {
    auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    CommandDispatcher<> disp{doc->context().commandStack};
    Device::DeviceSettings settings;
    settings.name = "window";
    settings.protocol = Gfx::WindowProtocolFactory::static_concreteKey();

    disp.submit<Explorer::Command::LoadDevice>(device_plug, settings);
  }

  // Simulate a drop of video files
  auto& scenario_dm = static_cast<Scenario::ScenarioDocumentModel&>(doc_pm);
  auto& root_itv = scenario_dm.baseInterval();

  {
    QMimeData mime;
    QList<QUrl> url;
    auto args = qApp->arguments();
    args.pop_front(); // Remove the binary name
    for(auto file : args)
    {
      if(QFile f{file}; f.exists())
      {
        url.push_back(QUrl::fromLocalFile(QFileInfo{f}.absoluteFilePath()));
      }
    }
    mime.setUrls(url);

    Scenario::DropProcessInInterval drop;
    drop.drop(doc->context(), root_itv, QPointF{}, mime);

    for(auto& inlets : root_itv.findChildren<Gfx::TextureOutlet*>())
    {
      inlets->setAddress(State::AddressAccessor{{{"window"}, {}}});
    }
  }

  // Start execution
  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&] {
                       eng.execution().play_interval(root_itv);
                     });
  // Stop execution
  QTimer::singleShot(5000, [&ctx, doc] {
                       ctx.docManager.forceCloseDocument(ctx, *doc);
                       qApp->exit(0); });
}

#define SCORE_INTEGRATION_TEST2(TestFun)                            \
                                                                   \
int main(int argc, char** argv)                                    \
{                                                                  \
  QLocale::setDefault(QLocale::C);                                 \
  std::setlocale(LC_ALL, "C");                                     \
                                                                   \
  score::MinimalGUIApplication app(argc, argv);                    \
                                                                   \
  QMetaObject::invokeMethod(&app, [] {                             \
    TestFun();                                                     \
    QApplication::processEvents();                                 \
  }, Qt::QueuedConnection);                                        \
                                                                   \
  return app.exec();                                               \
}

SCORE_INTEGRATION_TEST2(VideoTest)
