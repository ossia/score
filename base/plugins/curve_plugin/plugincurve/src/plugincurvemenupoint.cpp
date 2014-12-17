#include "../include/plugincurvemenupoint.h"
#include "../include/plugincurvepoint.hpp"


PluginCurveMenuPoint::PluginCurveMenuPoint (PluginCurvePoint* point, QWidget* parent) :
	QMenu (parent)
{
	QAction* vfixAction;
	addAction (DELETE);
	vfixAction = addAction (FIX_HORIZONTAL);
	vfixAction->setCheckable (true);
	vfixAction->setChecked ( (point->mobility() == Vertical) );
	addSeparator();
}
