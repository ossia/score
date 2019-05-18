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
		public score::Autoconnect_QtInterface,
@endif
@if "%FACTORYFAMILY%" == "true"
		public score::FactoryFamily_QtInterface,
@endif
@if "%FACTORYINTERFACE%" == "true"
		public score::FactoryInterface_QtInterface,
@endif
@if "%DOCUMENTDELEGATE%" == "true"
		public score::DocumentDelegateFactoryInterface_QtInterface,
@endif
@if "%PANEL%" == "true"
		public score::PanelFactoryInterface_QtInterface,
@endif
@if "%PLUGINCONTROL%" == "true"
		public score::PluginControlInterface_QtInterface,
@endif
@if "%SETTINGSDELEGATE%" == "true"
		public score::SettingsDelegateFactoryInterface_QtInterface,
@endif
		private Dummy
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
		Q_INTERFACES(
@if "%AUTOCONNECT%" == "true"
			score::Autoconnect_QtInterface
@endif
@if "%FACTORYFAMILY%" == "true"
			score::FactoryFamily_QtInterface
@endif
@if "%FACTORYINTERFACE%" == "true"
			score::FactoryInterface_QtInterface
@endif
@if "%DOCUMENTDELEGATE%" == "true"
			score::DocumentDelegateFactoryInterface_QtInterface
@endif
@if "%PANEL%" == "true"
			score::PanelFactoryInterface_QtInterface
@endif
@if "%PLUGINCONTROL%" == "true"
			score::PluginControlInterface_QtInterface
@endif
@if "%SETTINGSDELEGATE%" == "true"
			score::SettingsDelegateFactoryInterface_QtInterface
@endif
		)

	public:
		%ClassName%();
		virtual ~%ClassName%() = default;

@if "%AUTOCONNECT%" == "true"
		virtual QList<score::Autoconnect> autoconnect_list() const override;
@endif
		
@if "%FACTORYFAMILY%" == "true"
		virtual QVector<score::FactoryFamily> factoryFamilies_make() override;
@endif

@if "%FACTORYINTERFACE%" == "true"
		virtual QVector<score::FactoryInterface*> factories_make(QString factoryName) override;
@endif

@if "%DOCUMENTDELEGATE%" == "true"
		virtual QStringList document_list() const override;
		virtual score::DocumentDelegateFactoryInterface* document_make(QString name) override;
@endif
		
@if "%PANEL%" == "true"
		virtual QStringList panel_list() const override;
		virtual score::PanelFactoryInterface* panel_make (QString name) override;
@endif

@if "%PLUGINCONTROL%" == "true"
		virtual QStringList control_list() const override;
		virtual score::PluginControlInterface* control_make(QString) override;
@endif
		
@if "%SETTINGSDELEGATE%" == "true"
                virtual score::SettingsDelegateFactoryInterface* settings_make() override;
@endif
};
