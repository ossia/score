#include "%ClassName%.hpp"

%ClassName%::%ClassName%() :
    QObject{}
{
}

@if "%AUTOCONNECT%" == "true"
QList<iscore::Autoconnect> %ClassName%::autoconnect_list() const
{
	return
	{
	};
}
@endif

@if "%FACTORYFAMILY%" == "true"
QVector<iscore::FactoryFamily> %ClassName%::factoryFamilies_make()
{
	return {};
}
@endif

@if "%FACTORYINTERFACE%" == "true"
QVector<iscore::FactoryInterface*> %ClassName%::factories_make(QString factoryName)
{
	return {};
}
@endif

@if "%DOCUMENTDELEGATE%" == "true"
QStringList %ClassName%::document_list() const
{
	return {};
}

iscore::DocumentDelegateFactoryInterface* %ClassName%::document_make(QString name)
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

iscore::PluginControlInterface* %ClassName%::control_make(QString name)
{
	return nullptr;
}
@endif

@if "%SETTINGSDELEGATE%" == "true"
iscore::SettingsDelegateFactoryInterface* %ClassName%::settings_make()
{
	return nullptr;
}
@endif
