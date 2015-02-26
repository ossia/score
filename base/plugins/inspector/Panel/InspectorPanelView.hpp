#pragma once
#include <interface/panel/PanelViewInterface.hpp>
namespace iscore
{
    class View;
}
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
        void on_setNewItem(QObject*);

    private:
        InspectorPanel* m_panelWidget {};
};
