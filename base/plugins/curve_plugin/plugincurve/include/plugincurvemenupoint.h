#ifndef PLUGINCURVEMENUPOINT_H
#define PLUGINCURVEMENUPOINT_H

#include <QMenu>

class PluginCurvePoint;

class PluginCurveMenuPoint : public QMenu
{
		Q_OBJECT
	public:
		static const QString DELETE;
		static const QString FIX_HORIZONTAL;
		PluginCurveMenuPoint (PluginCurvePoint* point, QWidget* parent = 0);

	signals:

	public slots:

};

#endif // PLUGINCURVEMENUPOINT_H
