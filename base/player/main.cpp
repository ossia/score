#include <QCoreApplication>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <core/document/DocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentContext.hpp>

int main(int argc, char** argv)
{
  using namespace iscore;
  /*
  SafeQApplication app{argc, argv};

  iscore::ApplicationSettings glob_settings;
  iscore::ApplicationComponentsData data;
  // Load plug-ins

  ApplicationComponents comps{data};
  std::vector<std::unique_ptr<iscore::SettingsDelegateModel>> setgs;
  iscore::ApplicationContext appctx{glob_settings, comp, std::move(setgs)};
  DocumentBuilder builder;
  DocumentManager m;

  auto doc_factory = new ScenarioDocumentFactory;
  auto doc = new DocumentModel{};

  // Load plugins

  // Load document
  builder.loadDocument(ctx, {}, {});

  // initialize LocalTree::DocumentPlugin
  // injitialize execution document plugin
  // base scenario

  // Setup execution
  // clock
  auto& exec_settings = appctx.settings<Engine::Execution::Settings::Model>();
  auto clock = exec_settings.makeClock(exec_ctx);
  */


}
