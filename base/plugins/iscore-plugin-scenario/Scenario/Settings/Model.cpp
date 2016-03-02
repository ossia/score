#include "Model.hpp"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>

#include <Process/Style/Skin.hpp>
namespace Scenario
{
namespace Settings
{

const QString Keys::skin = QStringLiteral("Skin/ExecutionRate");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::skin))
    {
        setFirstTimeSettings();
    }
    else
    {
        setSkin(s.value(Keys::skin).toString());
    }
}

QString Model::getSkin() const
{
    return m_skin;
}

void Model::setSkin(const QString& skin)
{
    if (m_skin == skin)
        return;

    QFile f(":/DefaultSkin.json");
    if(skin == QStringLiteral("IEEE"))
    {
        f.setFileName(":/IEEESkin.json");
    }

    if(f.open(QFile::ReadOnly))
    {
        auto arr = f.readAll();
        auto err = new QJsonParseError;
        auto doc = QJsonDocument::fromJson(arr, err);
        if(err->error)
        {
            qDebug() << "could not load skin : "<< err->errorString() << err->offset;
        }
        else
        {
            auto obj = doc.object();
            Skin::instance().load(obj);
        }
    }
    else
    {
        qDebug() << "could not open" << f.fileName();
    }

    m_skin = skin;

    QSettings s;
    s.setValue(Keys::skin, m_skin);
    emit skinChanged(skin);
}

void Model::setFirstTimeSettings()
{
    m_skin = "Default";

    QSettings s;
    s.setValue(Keys::skin, m_skin);
}

}
}
