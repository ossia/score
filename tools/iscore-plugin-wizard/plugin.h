#pragma once
#include <QObject>
@if "%AUTOCONNECT%" == "true"
#include <interface/plugins/Autoconnect_QtInterface.hpp>
@endif
@if "%FACTORYFAMILY%" == "true"
#include <interface/plugins/FactoryFamily_QtInterface.hpp>
@endif
@if "%FACTORYINTERFACE%" == "true"
#include <interface/plugins/CustomFactoryInterface_QtInterface.hpp>
@endif
@if "%DOCUMENTDELEGATE%" == "true"
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
@endif
@if "%PANEL%" == "true"
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
@endif
@if "%PLUGINCONTROL%" == "true"
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
@endif
@if "%SETTINGSDELEGATE%" == "true"
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>
@endif

class Dummy{};
class %ClassName%:
		public QObject,
@if "%AUTOCONNECT%" == "true"
		public iscore::Autoconnect_QtInterface,
@endif
@if "%FACTORYFAMILY%" == "true"
		public iscore::FactoryFamily_QtInterface,
@endif
@if "%FACTORYINTERFACE%" == "true"
		public iscore::FactoryInterface_QtInterface,
@endif
@if "%DOCUMENTDELEGATE%" == "true"
		public iscore::DocumentDelegateFactoryInterface_QtInterface,
@endif
@if "%PANEL%" == "true"
		public iscore::PanelFactoryInterface_QtInterface,
@endif
@if "%PLUGINCONTROL%" == "true"
		public iscore::PluginControlInterface_QtInterface,
@endif
@if "%SETTINGSDELEGATE%" == "true"
		public iscore::SettingsDelegateFactoryInterface_QtInterface,
@endif
		private Dummy
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
		Q_INTERFACES(
@if "%AUTOCONNECT%" == "true"
			iscore::Autoconnect_QtInterface
@endif
@if "%FACTORYFAMILY%" == "true"
			iscore::FactoryFamily_QtInterface
@endif
@if "%FACTORYINTERFACE%" == "true"
			iscore::FactoryInterface_QtInterface
@endif
@if "%DOCUMENTDELEGATE%" == "true"
			iscore::DocumentDelegateFactoryInterface_QtInterface
@endif
@if "%PANEL%" == "true"
			iscore::PanelFactoryInterface_QtInterface
@endif
@if "%PLUGINCONTROL%" == "true"
			iscore::PluginControlInterface_QtInterface
@endif
@if "%SETTINGSDELEGATE%" == "true"
			iscore::SettingsDelegateFactoryInterface_QtInterface
@endif
		)

	public:
		%ClassName%();
		virtual ~%ClassName%() = default;

@if "%AUTOCONNECT%" == "true"
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;
@endif
		
@if "%FACTORYFAMILY%" == "true"
		virtual QVector<iscore::FactoryFamily> factoryFamilies_make() override;
@endif

@if "%FACTORYINTERFACE%" == "true"
		virtual QVector<iscore::FactoryInterface*> factories_make(QString factoryName) override;
@endif

@if "%DOCUMENTDELEGATE%" == "true"
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;
@endif
		
@if "%PANEL%" == "true"
		virtual QStringList panel_list() const override;
		virtual iscore::PanelFactoryInterface* panel_make (QString name) override;
@endif

@if "%PLUGINCONTROL%" == "true"
		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make(QString) override;
@endif
		
@if "%SETTINGSDELEGATE%" == "true"
		virtual iscore::SettingsDelegateFactoryInterface* settings_make() override
@endif
};
