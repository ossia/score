#ifndef PLUGINCURVEMENUPOINT_H
#define PLUGINCURVEMENUPOINT_H

#include <QMenu>
#include <QObject>
class PluginCurvePoint;
#define MENUPOINT_DELETE_TEXT QObject::tr("Delete")
#define MENUPOINT_FIX_HORIZONTAL_TEXT QObject::tr("Fix Horizontaly")

class PluginCurveMenuPoint : public QMenu
{
    public:
        const QString DELETE
        {
            MENUPOINT_DELETE_TEXT
        };
        const QString FIX_HORIZONTAL
        {
            MENUPOINT_FIX_HORIZONTAL_TEXT
        };

        PluginCurveMenuPoint (PluginCurvePoint* point, QWidget* parent = 0);
        virtual ~PluginCurveMenuPoint() = default;

};

#endif // PLUGINCURVEMENUPOINT_H
