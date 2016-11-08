#include <QApplication>
#include <QDebug>
#include <qnamespace.h>
#include <QSet>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringList>
#include <QVariant>

#include <ossia/detail/algorithms.hpp>
#include "PluginSettingsModel.hpp"
#include "commands/BlacklistCommand.hpp"
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
namespace PluginSettings
{
PluginSettingsModel::PluginSettingsModel(QSettings& set, const iscore::ApplicationContext& ctx) :
    iscore::SettingsDelegateModel {},
    localPlugins{ctx.components.addons()},
    remoteSelection{&remotePlugins}
{
}
}
