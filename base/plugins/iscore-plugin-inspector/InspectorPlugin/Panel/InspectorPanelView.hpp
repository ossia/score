#pragma once
#include <iscore/plugins/panel/PanelView.hpp>
#include <iscore/selection/Selection.hpp>

#include <QString>

class QWidget;

namespace iscore
{
    class Document;
    class View;
}
class InspectorPanel;
class SelectionStackWidget;

class InspectorPanelView : public iscore::PanelView
{
        Q_OBJECT
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        explicit InspectorPanelView(iscore::View* parent);

        QWidget* getWidget() override;

        const QString shortcut() const override
        { return tr("Ctrl+I"); }

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
