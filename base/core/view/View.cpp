#include <core/view/View.hpp>
#include <interface/panels/Panel.hpp>
#include <QDockWidget>
#include <QGridLayout>

using namespace iscore;

void View::addPanel(PanelView* v)
{
	QDockWidget * dial = new QDockWidget(this);
	v->setParent(this);
	m_panelsViews.insert(v);
	
	dial->setWidget(v->getWidget());
	
	this->addDockWidget(Qt::RightDockWidgetArea, dial);
}
