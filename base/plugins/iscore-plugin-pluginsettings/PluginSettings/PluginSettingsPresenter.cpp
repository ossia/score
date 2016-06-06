#include <QApplication>
#include <QDebug>
#include <QListView>
#include <QStandardItemModel>
#include <QStyle>

#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <iscore/command/Command.hpp>
#include <PluginSettings/FileDownloader.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include "PluginSettings/commands/BlacklistCommand.hpp"
#include <QStandardPaths>
#include <QBuffer>
#include <QFile>
#include <quazip/quazipfile.h>

namespace iscore {
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
void extractAll( QuaZip& archive, const QString& dirname )
{
    // Taken from http://www.qtcentre.org/threads/55561-Using-quazip-for-extracting-multiple-files
    for( bool f = archive.goToFirstFile(); f; f = archive.goToNextFile() )
    {
        QString filePath = archive.getCurrentFileName();
        QuaZipFile zFile( archive.getZipName(), filePath );

        zFile.open( QIODevice::ReadOnly );
        QByteArray ba = zFile.readAll();
        zFile.close();

        QFile dstFile( dirname + "/" + filePath );
        dstFile.open( QIODevice::WriteOnly | QIODevice::Text );
        dstFile.write( ba.data() );
        dstFile.close();
    }
}

PluginSettingsPresenter::PluginSettingsPresenter(
        iscore::SettingsDelegateModel& model,
        iscore::SettingsDelegateView& view,
        QObject* parent) :
    SettingsDelegatePresenter {model, view, parent}
{
    auto& ps_model = static_cast<PluginSettingsModel&>(model);
    auto& ps_view  = static_cast<PluginSettingsView&>(view);

    ps_view.localView()->setModel(&ps_model.localPlugins);
    ps_view.localView()->setColumnWidth(0, 150);
    ps_view.localView()->setColumnWidth(1, 400);
    ps_view.localView()->setColumnWidth(2, 400);

    ps_view.remoteView()->setModel(&ps_model.remotePlugins);
    ps_view.remoteView()->setColumnWidth(0, 150);
    ps_view.remoteView()->setColumnWidth(1, 400);

    connect(&ps_model.remoteSelection, &QItemSelectionModel::selectionChanged,
            this, [&] (const QItemSelection &selected,
                      const QItemSelection &deselected) {

        auto b = !selected.empty();
        if(b)
        {
            auto num = selected.indexes().first().row();
            RemoteAddon& addon = ps_model.remotePlugins.addons().at(num);

            auto it = addon.architectures.find(iscore::addonArchitecture());
            if(it != addon.architectures.end())
            {
                auto dl = new iscore::FileDownloader{it->second};
                connect(dl, &iscore::FileDownloader::downloaded,
                        this, [&,dl,addon] (QByteArray arr) {

                    QBuffer b{&arr};
                    QuaZip z{&b};

#if defined(ISCORE_DEPLOYMENT_BUILD)
                    auto docs = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
                    auto dirname = docs + "/i-score/plugins/";
#else
                    auto dirname = "addons";
#endif

                    extractAll(z, dirname);

                    dl->deleteLater();
                });

            }
        }
        ps_view.installButton().setEnabled(b);
    });


/*
    con(ps_model,	&PluginSettingsModel::blacklistCommand,
    this,		&PluginSettingsPresenter::setBlacklistCommand);*/
}


QIcon PluginSettingsPresenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
}
