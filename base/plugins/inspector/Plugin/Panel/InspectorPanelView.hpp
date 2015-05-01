#pragma once
#include <iscore/plugins/panel/PanelViewInterface.hpp>
#include <iscore/selection/Selection.hpp>
namespace iscore
{
    class Document;
    class View;
}
class SelectionStackWidget;
class InspectorPanel;
class InspectorPanelView : public iscore::PanelViewInterface
{
        Q_OBJECT
    public:
        InspectorPanelView(iscore::View* parent);

        QWidget* getWidget() override;
        Qt::DockWidgetArea defaultDock() const override;
        int priority() const override;
        QString prettyName() const override;


    public slots:
        void setCurrentDocument(iscore::Document*);
        void setNewSelection(const Selection& s);

    private:
        QWidget* m_widget{};
        SelectionStackWidget* m_stack{};
        InspectorPanel* m_inspectorPanel {};
        iscore::Document* m_currentDocument{};

        QMetaObject::Connection m_selectionConnection,
                                m_commandConnection;
};
