#include "iscore_plugin_audio.hpp"
#include <Audio/AudioFactory.hpp>
#include <Audio/AudioDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <core/document/DocumentModel.hpp>
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
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                Audio::ProcessFactory>();
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
class ApplicationPlugin : public QObject, public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(const iscore::ApplicationContext& app):
            iscore::GUIApplicationContextPlugin{app, "AudioApplicationPlugin", nullptr}
        {

        }

        void on_newDocument(iscore::Document* doc) override
        {
            auto plug = new AudioDocumentPlugin{*doc, &doc->model()};
            doc->model().addPluginModel(plug);

            auto& ctrl = doc->context().app.components.applicationPlugin<ScenarioApplicationPlugin>();
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
iscore::GUIApplicationContextPlugin*iscore_plugin_audio::make_applicationPlugin(const iscore::ApplicationContext& app)
{
    return new Audio::ApplicationPlugin{app};
}
