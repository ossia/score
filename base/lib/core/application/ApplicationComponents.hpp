#pragma once
#include <vector>
#include <unordered_map>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class PluginControlInterface;
class FactoryListInterface;
class PanelFactory;
class PanelPresenter;

struct ApplicationComponentsData
{
        ~ApplicationComponentsData();

        std::vector<PluginControlInterface*> controls;
        std::vector<DocumentDelegateFactoryInterface*> availableDocuments;
        std::unordered_map<iscore::FactoryBaseKey, FactoryListInterface*> factories;
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap> commands;

        // TODO instead put the factory as a member function?
        QList<QPair<PanelPresenter*,
                    PanelFactory*>> panelPresenters;
};

class ApplicationComponents
{
    public:
        ApplicationComponents(const iscore::ApplicationComponentsData& d):
            m_data(d)
        {

        }


        // Getters for plugin-registered things
        const auto& pluginControls() const
        { return m_data.controls; }
        const auto& availableDocuments() const
        { return m_data.availableDocuments; }
        const auto& controls() const // TODO REMOVEME
        { return m_data.controls; }

        template<typename T>
        T* control() const
        {
            for(auto& elt : m_data.controls)
            {
                if(auto c = dynamic_cast<T*>(elt))
                {
                    return c;
                }
            }

            ISCORE_ABORT;
            return nullptr;
        }
        const auto& panelPresenters() const
        { return m_data.panelPresenters; }
        auto panelFactories() const
        {
            QList<PanelFactory*> lst;
            std::transform(
                        std::begin(m_data.panelPresenters),
                        std::end(m_data.panelPresenters),
                        std::back_inserter(lst),
                [] (const QPair<PanelPresenter*, PanelFactory*>& elt) {
                    return elt.second;
                }
            );
            return lst;
        }

        template<typename T>
        const T* factory() const
        {
            auto it = m_data.factories.find(T::staticFactoryKey());
            if(it != m_data.factories.end())
            {
                return safe_cast<T*>(it->second);
            }
            return nullptr;
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
        instantiateUndoCommand(const CommandParentFactoryKey& parent_name,
                               const CommandFactoryKey& name,
                               const QByteArray& data) const;

    private:
        const iscore::ApplicationComponentsData& m_data;
};
}
