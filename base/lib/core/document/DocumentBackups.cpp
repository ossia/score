#include "DocumentBackups.hpp"

#include <core/application/OpenDocumentsFile.hpp>
#include <QMessageBox>
#include <QFile>
#include <QSettings>
#include <QApplication>
bool iscore::DocumentBackups::canRestoreDocuments()
{
    // Try to reload if there was a crash
    if(OpenDocumentsFile::exists())
    {
        if(QMessageBox::question(
                    qApp->activeWindow(),
                    QObject::tr("Reload?"),
                    QObject::tr("It seems that i-score previously crashed. Do you wish to reload your work?")) == QMessageBox::Yes)
        {
            return true;
        }
        else
        {
            DocumentBackups::clear();
            return false;
        }
    }

    return false;
}

template<typename T>
static void loadRestorableDocumentData(
        const QString& date_filename,
        const QString& command_filename,
        T& arr)
{
    QFile data_file{date_filename};
    QFile command_file{command_filename};
    if(data_file.exists() && command_file.exists())
    {
        data_file.open(QFile::ReadOnly);
        command_file.open(QFile::ReadOnly);

        arr.push_back({data_file.readAll(), command_file.readAll()});

        data_file.close();
        data_file.remove(); // TODO maybe we don't want to remove them that early?

        command_file.close();
        command_file.remove();
    }
}

std::vector<std::pair<QByteArray, QByteArray> >
iscore::DocumentBackups::restorableDocuments()
{
    std::vector<std::pair<QByteArray, QByteArray>> arr;
    QSettings s{iscore::OpenDocumentsFile::path(), QSettings::IniFormat};

    auto existing_files = s.value("iscore/docs").toMap();

    for(const auto& element : existing_files.keys())
    {
        loadRestorableDocumentData(element, existing_files[element].toString(), arr);
    }

    s.setValue("iscore/docs", QMap<QString, QVariant>{});
    s.sync();

    return arr;
}

void iscore::DocumentBackups::clear()
{
    if(OpenDocumentsFile::exists())
    {
        // Remove all the tmp files
        QSettings s{iscore::OpenDocumentsFile::path(), QSettings::IniFormat};

        auto existing_files = s.value("iscore/docs").toMap();

        for(const auto& element : existing_files.keys())
        {
            QFile{element}.remove();
            QFile{existing_files[element].toString()}.remove();
        }

        // Remove the file containing the map
        QFile{OpenDocumentsFile::path()}.remove();
    }
}
