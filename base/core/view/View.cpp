#include <core/view/View.hpp>
#include <interface/panels/Panel.hpp>

using namespace iscore;

void View::addPanel(PanelView* v)
{
	v->setParent(this);
	m_panelsViews.insert(v);
}
