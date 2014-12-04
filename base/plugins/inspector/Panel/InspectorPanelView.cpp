#include "InspectorPanelView.hpp"
#include <core/view/View.hpp>

#include "Implementation/InspectorPanel.hpp"
InspectorPanelView::InspectorPanelView (iscore::View* parent) :
	iscore::PanelViewInterface{parent}
{
	m_panelWidget = new InspectorPanel{parent};
}

QWidget* InspectorPanelView::getWidget()
{
	
//	ObjectInterval* test1 = new ObjectInterval ("MonNom", "remarques diverses", Qt::red );

//	ptr->newItemInspected (test1);

	return m_panelWidget;
}

void InspectorPanelView::on_setNewItem(QObject* obj)
{
	m_panelWidget->newItemInspected(obj);
}
