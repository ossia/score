#pragma once
#include <iscore/command/CommandData.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_lib_base_export.h>

#include <QByteArray>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>


namespace iscore
{
class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class GUIApplicationContextPlugin;
class PanelFactory;
class PanelPresenter;

struct ApplicationComponentsData
{
        // TODO Forbid copy, etc... (in ALL types!!)
        ~ApplicationComponentsData();

        QStringList pluginFiles;
        std::vector<QObject*> plugins;
        std::vector<GUIApplicationContextPlugin*> appPlugins;
        std::vector<DocumentDelegateFactoryInterface*> availableDocuments;
        std::unordered_map<iscore::FactoryBaseKey, std::unique_ptr<FactoryListInterface>> factories;
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap> commands;

        // TODO instead put the factory as a member function?
        std::vector<std::pair<PanelPresenter*, PanelFactory*>> panelPresenters;
};

class ISCORE_LIB_BASE_EXPORT ApplicationComponents
{
    public:
        ApplicationComponents(const iscore::ApplicationComponentsData& d):
            m_data(d)
        {

        }


        // Getters for plugin-registered things
        const auto& availableDocuments() const
        { return m_data.availableDocuments; }
        const auto& applicationPlugins() const // TODO REMOVEME
        { return m_data.appPlugins; }

        template<typename T>
        T& applicationPlugin() const
        {
            for(auto& elt : m_data.appPlugins)
            {
                if(auto c = dynamic_cast<T*>(elt))
                {
                    return *c;
                }
            }

            ISCORE_ABORT;
            throw;
        }

        const auto& panelPresenters() const
        { return m_data.panelPresenters; }
        auto panelFactories() const
        {
            std::vector<PanelFactory*> lst;
            std::transform(
                        std::begin(m_data.panelPresenters),
                        std::end(m_data.panelPresenters),
                        std::back_inserter(lst),
                [] (const std::pair<PanelPresenter*, PanelFactory*>& elt) {
                    return elt.second;
                }
            );
            return lst;
        }

        template<typename T>
        const T& factory() const
        {
            auto it = m_data.factories.find(T::staticFactoryKey());
            if(it != m_data.factories.end())
            {
                return *safe_cast<T*>(it->second.get());
            }

            ISCORE_ABORT;
            throw;
        }

        /**
         * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
         * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
         * @param name The name of the command to generate.
         * @param data The data of the command.
         *
         * Ownership of the command is transferred to the caller, and he must delete it.
         */
        iscore::SerializableCommand*
        instantiateUndoCommand(const CommandData& cmd) const;

    private:
        const iscore::ApplicationComponentsData& m_data;
};
}
