#pragma once
class IScorePanel
{
};

class IScorePanelFactoryPluginInterface
{
	public:
		// List the panels offered by the plugin.
		// Note : here we don't use camel case for the "panel_" part because it is more of a namespacing required 
		// by the fact that if a plugin implements for instance a process and a panel, 
		// Qt requires the creation of the class which inherits both the process interface and the panel interface
		// which might cause name collisions.
		QStringList panel_list() = 0;
		std::unique_ptr<IScorePanel> panel_make(QString name) = 0;
};