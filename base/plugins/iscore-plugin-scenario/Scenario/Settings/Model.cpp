#include "Model.hpp"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Process/Style/Skin.hpp>

namespace Scenario
{
namespace Settings
{
namespace Keys
{
        const sp<ModelSkinParameter> skin{QStringLiteral("Skin/Skin"), "Default"};
        const sp<ModelGraphicZoomParameter> graphicZoom{QStringLiteral("Skin/Zoom"), 1};
        const sp<ModelSlotHeightParameter> slotHeight{QStringLiteral("Skin/slotHeight"), 400};
        const sp<ModelDefaultScoreDurationParameter> defaultDuration{QStringLiteral("Skin/defaultDuration"), TimeValue::fromMsecs(15000)};

        auto settings() {
            return std::tie(skin, graphicZoom, slotHeight, defaultDuration);
        }

        template<typename T, typename Model>
        void checkDefaultSettings(QSettings& set, const T& tuple, Model& model)
        {
            for_each_in_tuple(tuple, [&] (auto& e) {
                using type = std::remove_reference_t<decltype(e)>;
                using data_type = typename type::data_type;
                using param_type = typename type::parameter_type;
                if(!set.contains(e.key))
                {
                    set.setValue(e.key, QVariant::fromValue(e.def));
                    (model.*param_type::set())(e.def);
                }
                else
                {
                    auto val = set.value(e.key).template value<data_type>();
                    (model.*param_type::set())(val);
                }
            });
        }
}

Model::Model(const iscore::ApplicationContext& ctx)
{
    QSettings set;
    Keys::checkDefaultSettings(set, Keys::settings(), *this);
/*
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
        setDefaultScoreDuration(s.value(Keys::defaultDuration).value<TimeValue>());
    }
*/
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
    s.setValue(Keys::skin.key, m_skin);
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
    s.setValue(Keys::graphicZoom.key, m_graphicZoom);
    emit GraphicZoomChanged(m_graphicZoom);
}

void Model::setFirstTimeSettings()
{
    setSkin("Default");
    m_graphicZoom = 1.;
    m_slotHeight = 400;
    m_defaultDuration = TimeValue::fromMsecs(15000);
    QSettings s;
    s.setValue(Keys::graphicZoom.key, m_graphicZoom);
    s.setValue(Keys::skin.key, m_skin);
    s.setValue(Keys::slotHeight.key, m_slotHeight);
    s.setValue(Keys::defaultDuration.key, QVariant::fromValue(m_defaultDuration));
}

qreal Model::getSlotHeight() const
{
    return m_slotHeight;
}

void Model::setSlotHeight(qreal slotHeight)
{
    if(slotHeight == m_slotHeight)
        return;

    m_slotHeight = slotHeight;

    QSettings s;
    s.setValue(Keys::slotHeight.key, m_slotHeight);
    emit SlotHeightChanged(m_slotHeight);
}

TimeValue Model::getDefaultScoreDuration()
{
    return m_defaultDuration;
}

void Model::setDefaultScoreDuration(const TimeValue& dur)
{
    if(dur == m_defaultDuration)
        return;

    m_defaultDuration = dur;

    QSettings s;
    s.setValue(Keys::defaultDuration.key, QVariant::fromValue(m_defaultDuration));
    emit DefaultScoreDurationChanged(m_defaultDuration);
}

}
}
