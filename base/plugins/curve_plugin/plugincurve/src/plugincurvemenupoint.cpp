#include "plugincurvemenupoint.h"
#include "plugincurvepoint.hpp"

const QString PluginCurveMenuPoint::DELETE = "Delete";
const QString PluginCurveMenuPoint::FIX_HORIZONTAL = "Fix Horizontaly";


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
