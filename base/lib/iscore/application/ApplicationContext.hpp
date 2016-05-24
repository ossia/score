#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
struct ApplicationSettings;
class SettingsDelegateModelInterface;
class DocumentManager;
class MenuManager;
class ToolbarManager;
class ActionManager;

struct ApplicationContext
{
        explicit ApplicationContext(
                const iscore::ApplicationSettings&,
                const ApplicationComponents&,
                DocumentManager&,
                iscore::MenuManager&,
                iscore::ToolbarManager&,
                iscore::ActionManager&,
                const std::vector<iscore::SettingsDelegateModelInterface*>&);
        ApplicationContext(const ApplicationContext&) = delete;
        ApplicationContext(ApplicationContext&&) = delete;
        ApplicationContext& operator=(const ApplicationContext&) = delete;

        template<typename T>
        T& settings() const
        {
            for(auto& elt : this->m_settings)
            {
                if(auto c = dynamic_cast<T*>(elt))
                {
                    return *c;
                }
            }

            ISCORE_ABORT;
            throw;
        }

        const iscore::ApplicationSettings& applicationSettings;
        const iscore::ApplicationComponents& components;
        DocumentManager& documents;

        MenuManager& menus;
        ToolbarManager& toolbars;
        ActionManager& actions;

    private:
        const std::vector<iscore::SettingsDelegateModelInterface*>& m_settings;
};

// By default this is defined in iscore::Application
ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext();
}
