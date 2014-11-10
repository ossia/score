#include "PluginSettingsModel.hpp"
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <core/plugin/PluginManager.hpp>
#include "PluginSettingsPresenter.hpp"
#include "commands/BlacklistCommand.hpp"

using namespace iscore;

PluginSettingsModel::PluginSettingsModel():
	iscore::SettingsDelegateModelInterface{}
{
	this->setObjectName("PluginSettingsModel");

	QSettings s;
	auto blacklist = s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
	blacklist.sort();
	auto systemlist = qApp->findChild<PluginManager*>("PluginManager")->pluginsOnSystem();
	systemlist.sort();

	m_plugins = new QStandardItemModel(1, 1, this);

	int i = 0;
	for(auto& plugin_name : systemlist)
	{
		QStandardItem *item = new QStandardItem(plugin_name);
		item->setCheckable(true);
		item->setCheckState(blacklist.contains(plugin_name)? Qt::Checked : Qt::Unchecked);

		m_plugins->setItem(i++, 0, item);
	}

	auto diff = blacklist.toSet() - systemlist.toSet(); // The ones in the blacklist but not in the systemlist
	for(auto& plugin_name : diff)
	{
		QStandardItem *item = new QStandardItem(plugin_name);
		item->setCheckable(true);
		item->setCheckState(Qt::Checked);
	}

	connect(m_plugins,  &QStandardItemModel::itemChanged,
			this,		&PluginSettingsModel::on_itemChanged);
}

void PluginSettingsModel::setPresenter(SettingsDelegatePresenterInterface* presenter)
{
	m_presenter = static_cast<PluginSettingsPresenter*>(presenter);
}

void PluginSettingsModel::setFirstTimeSettings()
{
}

void PluginSettingsModel::on_itemChanged(QStandardItem* it)
{
	// Créer une commande qui change le QSettings. Note : si possible avec un merge.
	auto name = it->text();
	qDebug() << name << it->checkState();

	presenter()->setBlacklistCommand(new BlacklistCommand(it->text(), it->checkState()));
}
