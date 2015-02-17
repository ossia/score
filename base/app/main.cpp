#include <iostream>
#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(ScenarioPlugin)
Q_IMPORT_PLUGIN(InspectorPlugin)
Q_IMPORT_PLUGIN(DeviceExplorerPlugin)
Q_IMPORT_PLUGIN(PluginSettingsPlugin) // static plug-ins should not be displayed.
Q_IMPORT_PLUGIN(NetworkPlugin)
Q_IMPORT_PLUGIN(CurvePlugin)
Q_IMPORT_PLUGIN(IScoreCohesion)
#endif

#include <QStyleFactory>
int main(int argc, char **argv)
{
	iscore::Application app(argc, argv);

	/* Theming
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
