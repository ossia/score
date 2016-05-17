#pragma once
#include <iscore/command/CommandData.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore_lib_base_export.h>

#include <QByteArray>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>


namespace iscore
{
class DocumentDelegateFactory;
class DocumentPluginFactory;
class FactoryListInterface;
class Plugin_QtInterface;
class GUIApplicationContextPlugin;
class PanelDelegate;

struct ISCORE_LIB_BASE_EXPORT ApplicationComponentsData
{
        ~ApplicationComponentsData();
        ApplicationComponentsData() = default;
        ApplicationComponentsData(const ApplicationComponentsData&) = delete;
        ApplicationComponentsData(ApplicationComponentsData&&) = delete;
        ApplicationComponentsData& operator=(const ApplicationComponentsData&) = delete;
        ApplicationComponentsData& operator=(ApplicationComponentsData&&) = delete;

        QStringList pluginFiles;
        std::vector<iscore::Plugin_QtInterface*> plugins;
        std::vector<GUIApplicationContextPlugin*> appPlugins;

        std::unordered_map<iscore::AbstractFactoryKey, std::unique_ptr<FactoryListInterface>> factories;
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap> commands;
        std::vector<std::unique_ptr<PanelDelegate>> panels;
};

class ISCORE_LIB_BASE_EXPORT ApplicationComponents
{
    public:
        ApplicationComponents(const iscore::ApplicationComponentsData& d):
            m_data(d)
        {

        }


        // Getters for plugin-registered things
        const auto& applicationPlugins() const
        { return m_data.appPlugins; }
        const auto& plugins() const
        { return m_data.plugins; }

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

        auto panels() const
        { return wrap_indirect(m_data.panels); }

        template<typename T>
        const T& factory() const
        {
            static_assert(T::factory_list_tag, "This needs to be called with a factory list class");
            auto it = m_data.factories.find(T::static_abstractFactoryKey());
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
