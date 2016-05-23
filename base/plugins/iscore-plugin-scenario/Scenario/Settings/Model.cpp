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
const QString Keys::slotHeight = QStringLiteral("Skin/slotHeight");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::skin) ||
       !s.contains(Keys::graphicZoom))
    {
        Model::setFirstTimeSettings();
    }
    else
    {
        setSkin(s.value(Keys::skin).toString());
        setGraphicZoom(s.value(Keys::graphicZoom).toDouble());
        setSlotHeight(s.value(Keys::slotHeight).toReal());
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
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(arr, &err);
        if(err.error)
        {
            qDebug() << "could not load skin : "<< err.errorString() << err.offset;
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
    emit SkinChanged(skin);
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

    QSettings s;
    s.setValue(Keys::graphicZoom, m_graphicZoom);
    emit GraphicZoomChanged(m_graphicZoom);
}

void Model::setFirstTimeSettings()
{
    setSkin("Default");
    m_graphicZoom = 1.;
    m_slotHeight = 400;
    QSettings s;
    s.setValue(Keys::graphicZoom, m_graphicZoom);
    s.setValue(Keys::skin, m_skin);
    s.setValue(Keys::slotHeight, m_slotHeight);
}

qreal Model::getSlotHeight() const
{
    return m_slotHeight;
}

void Model::setSlotHeight(const qreal& slotHeight)
{
    if(slotHeight == m_slotHeight)
        return;

    m_slotHeight = slotHeight;

    QSettings s;
    s.setValue(Keys::slotHeight, m_slotHeight);
    emit SlotHeightChanged(m_slotHeight);
}

}
}
