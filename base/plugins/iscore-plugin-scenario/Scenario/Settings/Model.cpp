#include "Model.hpp"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>

#include <Process/Style/Skin.hpp>
namespace Scenario
{
namespace Settings
{

const QString Keys::skin = QStringLiteral("Skin/Skin");
const QString Keys::graphicZoom = QStringLiteral("Skin/Zoom");


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

    if(!s.contains(Keys::graphicZoom))
        setFirstTimeSettings();
    else
        setGraphicZoom(s.value(Keys::graphicZoom).toDouble());
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

double Model::getGraphicZoom() const
{
    return m_graphicZoom;
}

void Model::setGraphicZoom(double graphicZoom)
{
    if (graphicZoom == m_graphicZoom)
        return;

    m_graphicZoom = graphicZoom;
    emit graphicZoomChanged(m_graphicZoom);
}

void Model::setFirstTimeSettings()
{
    setSkin("Default");
    m_graphicZoom = 1.;
}

}
}
