#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

#include <QTabWidget>
namespace iscore
{
    class View;
}

class LibraryPanelView : public iscore::PanelView
{
    public:
        LibraryPanelView(iscore::View* parent);

        QWidget* getWidget();
        Qt::DockWidgetArea defaultDock() const;
        int priority() const;
        QString prettyName() const;

    private:
        QTabWidget* m_widget{};
};
