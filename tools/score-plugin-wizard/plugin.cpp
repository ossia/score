#include "%ClassName%.hpp"

%ClassName%::%ClassName%() :
    QObject{}
{
}

@if "%AUTOCONNECT%" == "true"
QList<score::Autoconnect> %ClassName%::autoconnect_list() const
{
	return
	{
	};
}
@endif

@if "%FACTORYFAMILY%" == "true"
QVector<score::FactoryFamily> %ClassName%::factoryFamilies_make()
{
	return {};
}
@endif

@if "%FACTORYINTERFACE%" == "true"
QVector<score::FactoryInterface*> %ClassName%::factories_make(QString factoryName)
{
	return {};
}
@endif

@if "%DOCUMENTDELEGATE%" == "true"
QStringList %ClassName%::document_list() const
{
	return {};
}

score::DocumentDelegateFactoryInterface* %ClassName%::document_make(QString name)
{
	return nullptr;
}
@endif

@if "%PANEL%" == "true"
QStringList %ClassName%::panel_list() const
{
	return {};
}

PanelFactory* %ClassName%::panel_make (QString name)
{
	return nullptr;
}
@endif

@if "%PLUGINCONTROL%" == "true"
QStringList %ClassName%::control_list() const
{
	return {};
}

score::PluginControlInterface* %ClassName%::control_make(QString name)
{
	return nullptr;
}
@endif

@if "%SETTINGSDELEGATE%" == "true"
score::SettingsDelegateFactoryInterface* %ClassName%::settings_make()
{
	return nullptr;
}
@endif
