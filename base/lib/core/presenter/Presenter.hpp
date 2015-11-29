#pragma once
#include <core/application/ApplicationComponents.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <vector>

class QObject;


namespace iscore
{
    class View;


    /**
     * @brief The Presenter class
     *
     * Certainly needs refactoring.
     * For now, manages menus and plug-in objects.
     *
     * It is also able to instantiate a Command from serialized Undo/Redo data.
     * (this should go in the DocumentPresenter maybe ?)
     */
    class Presenter final : public NamedObject
    {
            Q_OBJECT
        public:
            Presenter(iscore::View* view, QObject* parent);

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

            auto& components()
            { return m_components; }
        private:
            void setupMenus();
            void on_activeWindowChanged();
            View* m_view {};

            DocumentManager m_docManager;
            ApplicationComponentsData m_components;
            ApplicationComponents m_components_readonly;

            MenubarManager m_menubar;

            std::vector<OrderedToolbar> m_toolbars;

    };
}
