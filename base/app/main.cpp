#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(iscore_plugin_scenario)
Q_IMPORT_PLUGIN(iscore_plugin_inspector)
Q_IMPORT_PLUGIN(iscore_plugin_deviceexplorer)
Q_IMPORT_PLUGIN(iscore_plugin_pluginsettings)  // static plug-ins should not be displayed.
Q_IMPORT_PLUGIN(iscore_plugin_curve)
#ifdef ISCORE_NETWORK
Q_IMPORT_PLUGIN(iscore_plugin_network)
#endif
#ifdef ISCORE_COHESION
Q_IMPORT_PLUGIN(iscore_plugin_cohesion)
#endif

#ifdef ISCORE_OSSIA
Q_IMPORT_PLUGIN(iscore_plugin_ossia)
#endif
#endif

#include <QStyleFactory>
class ScenarioPalette : public QObject
{

};

int main(int argc, char** argv)
{
    iscore::Application app(argc, argv);
    QPalette scenarPalette;
    scenarPalette.setBrush(QPalette::Background, QColor::fromRgb(37, 41, 48));

    scenarPalette.setBrush(QPalette::WindowText, QColor::fromRgb(222, 0, 0)); // Red
    scenarPalette.setBrush(QPalette::Button, QColor::fromRgb(109,222,0)); // Green
    scenarPalette.setBrush(QPalette::Base, QColor::fromRgb(3, 195, 221)); // Blue
    scenarPalette.setBrush(QPalette::AlternateBase, QColor::fromRgb(179, 179, 179)); // Grey
    scenarPalette.setBrush(QPalette::BrightText, QColor::fromRgb(255,225,0)); // Yellow

    qApp->setPalette(scenarPalette, "ScenarioPalette");
/*
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53,53,53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25,25,25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53,53,53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    qApp->setPalette(darkPalette);

    qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
*/
    return app.exec();
}
