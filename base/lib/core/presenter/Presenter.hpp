#pragma once
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <vector>

#include <iscore_lib_base_export.h>
class QObject;


namespace iscore
{
    class View;
    class Settings;

    /**
     * @brief The Presenter class
     *
     * Certainly needs refactoring.
     * For now, manages menus and plug-in objects.
     *
     * It is also able to instantiate a Command from serialized Undo/Redo data.
     * (this should go in the DocumentPresenter maybe ?)
     */
    class ISCORE_LIB_BASE_EXPORT Presenter final : public NamedObject
    {
            Q_OBJECT
        public:
            Presenter(
                    const iscore::ApplicationSettings& app,
                    const iscore::Settings& set,
                    iscore::View* view,
                    QObject* parent);

            // Exit i-score
            bool exit();

            // Toolbars
            std::vector<OrderedToolbar>& toolbars()
            { return m_toolbars; }


            View* view() const;

            auto& menuBar()
            { return m_menubar; }

            auto& documentManager()
            { return m_docManager; }
            const ApplicationComponents& applicationComponents()
            { return m_components_readonly; }
            const ApplicationContext& applicationContext()
            { return m_context; }

            auto& components()
            { return m_components; }
        private:
            void setupMenus();
            View* m_view {};
            const Settings& m_settings;

            DocumentManager m_docManager;
            ApplicationComponentsData m_components;
            ApplicationComponents m_components_readonly;

            MenubarManager m_menubar;
            ApplicationContext m_context;

            std::vector<OrderedToolbar> m_toolbars;

    };
}
