#ifndef PLUGINCURVEMENUSECTION_HPP
#define PLUGINCURVEMENUSECTION_HPP

#include <QMenu>
class PluginCurveSection;

class PluginCurveMenuSection : public QMenu
{
    Q_OBJECT
public:
    static const QString DELETE;
    explicit PluginCurveMenuSection(PluginCurveSection *section, QWidget *parent = 0);

signals:

public slots:

};

#endif // PLUGINCURVEMENUSECTION_HPP
