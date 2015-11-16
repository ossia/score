#pragma once
#include <core/presenter/MenubarManager.hpp>

#include <set>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <iscore/widgets/OrderedToolbar.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <unordered_map>


namespace iscore
{
    class SerializableCommand;
    class View;
    class Presenter;


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
            QList<OrderedToolbar>& toolbars()
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
            View* m_view {};

            DocumentManager m_docManager;
            ApplicationComponentsData m_components;
            ApplicationComponents m_components_readonly;

            MenubarManager m_menubar;

            QList<OrderedToolbar> m_toolbars;

    };
}
