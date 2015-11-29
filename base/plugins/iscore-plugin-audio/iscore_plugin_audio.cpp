#include "iscore_plugin_audio.hpp"
#include <Audio/AudioFactory.hpp>
#include <Audio/AudioDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/application/Application.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <QAction>

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_audio::make_commands()
{
    return {};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_audio::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new Audio::ProcessFactory};
    }
    return {};
}

iscore_plugin_audio::iscore_plugin_audio()
{

}

iscore_plugin_audio::~iscore_plugin_audio()
{

}
namespace Audio
{
class ApplicationPlugin : public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(iscore::Application& app):
            iscore::GUIApplicationContextPlugin{app, "AudioApplicationPlugin", &app}
        {

        }

        void on_newDocument(iscore::Document* doc) override
        {
            auto plug = new AudioDocumentPlugin{*doc, &doc->model()};
            doc->model().addPluginModel(plug);


            iscore::ApplicationContext ctx{iscore::Application::instance()};
            auto& ctrl = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
            auto acts = ctrl.actions();
            for(const auto& act : acts)
            {
                if(act->objectName() == "Play")
                {
                    connect(act, &QAction::toggled,
                            plug, [=] (bool b)
                    { plug->engine().play(); });
                }
                else if(act->objectName() == "Stop")
                {
                    connect(act, &QAction::triggered,
                            plug, [=] (bool b) { plug->engine().stop(); });
                }
            }
        }

};
}
iscore::GUIApplicationContextPlugin*iscore_plugin_audio::make_applicationPlugin(iscore::Application& app)
{
    return new Audio::ApplicationPlugin{app};
}
