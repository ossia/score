#pragma once
#include <interface/panel/PanelViewInterface.hpp>
#include <core/interface/selection/Selection.hpp>
namespace iscore
{
    class DocumentPresenter;
    class View;
}
class SelectionStackWidget;
class InspectorPanel;
class InspectorPanelView : public iscore::PanelViewInterface
{
        Q_OBJECT
    public:
        InspectorPanelView(iscore::View* parent);
        virtual QWidget* getWidget() override;

        virtual Qt::DockWidgetArea defaultDock() const
        {
            return Qt::RightDockWidgetArea;
        }

    signals:
        void newSelection(const Selection& s);

    public slots:
        void setCurrentDocument(iscore::DocumentPresenter*);
        void setNewSelection(const Selection& s);

    private:
        QWidget* m_widget{};
        SelectionStackWidget* m_stack{};
        InspectorPanel* m_inspectorPanel {};

        QMetaObject::Connection m_selectionConnection,
                                m_commandConnection;
};
