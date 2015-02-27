#pragma once
#include <interface/panel/PanelViewInterface.hpp>
namespace iscore
{
    class DocumentPresenter;
    class View;
}
class SelectionStackWidget;
class InspectorPanel;
class InspectorPanelView : public iscore::PanelViewInterface
{
    public:
        InspectorPanelView(iscore::View* parent);
        virtual QWidget* getWidget() override;

        virtual Qt::DockWidgetArea defaultDock() const
        {
            return Qt::RightDockWidgetArea;
        }

    public slots:
        //void on_setNewItem(QObject*);
        void setCurrentDocument(iscore::DocumentPresenter*);

    private:
        QWidget* m_widget{};
        SelectionStackWidget* m_stack{};
        InspectorPanel* m_inspectorPanel {};

        QMetaObject::Connection m_selectionConnection,
                                m_commandConnection;
};
