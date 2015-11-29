#include <core/plugin/PluginManager.hpp>
#include <qapplication.h>
#include <qdebug.h>
#include <qnamespace.h>
#include <qset.h>
#include <qsettings.h>
#include <qstandarditemmodel.h>
#include <qstringlist.h>
#include <qvariant.h>

#include "PluginSettingsModel.hpp"
#include "commands/BlacklistCommand.hpp"
#include "iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp"

using namespace iscore;

PluginSettingsModel::PluginSettingsModel() :
    iscore::SettingsDelegateModelInterface {}
{
    this->setObjectName("PluginSettingsModel");

    QSettings s;
    auto blacklist = s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
    blacklist.sort();

    ISCORE_TODO;
    return;


    auto systemlist = qApp->findChild<PluginLoader*> ("PluginManager")->pluginsOnSystem();
    systemlist.sort();

    m_plugins = new QStandardItemModel(1, 1, this);

    int i = 0;

    for(auto& plugin_name : systemlist)
    {
        QStandardItem* item = new QStandardItem(plugin_name);
        item->setCheckable(true);
        item->setCheckState(blacklist.contains(plugin_name) ? Qt::Checked : Qt::Unchecked);

        m_plugins->setItem(i++, 0, item);
    }

    auto diff = blacklist.toSet() - systemlist.toSet(); // The ones in the blacklist but not in the systemlist

    for(auto& plugin_name : diff)
    {
        QStandardItem* item = new QStandardItem(plugin_name);
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);
    }

    connect(m_plugins,  &QStandardItemModel::itemChanged,
    this,		&PluginSettingsModel::on_itemChanged);
}


void PluginSettingsModel::setFirstTimeSettings()
{
}

void PluginSettingsModel::on_itemChanged(QStandardItem* it)
{
    // Créer une commande qui change le QSettings. Note : si possible avec un merge.
    auto name = it->text();
    qDebug() << name << it->checkState();

    emit blacklistCommand(new BlacklistCommand(it->text(), it->checkState()));
}
