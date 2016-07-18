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
namespace Parameters
{
        const iscore::sp<ModelSkinParameter> Skin{QStringLiteral("Skin/Skin"), "Default"};
        const iscore::sp<ModelGraphicZoomParameter> GraphicZoom{QStringLiteral("Skin/Zoom"), 1};
        const iscore::sp<ModelSlotHeightParameter> SlotHeight{QStringLiteral("Skin/slotHeight"), 200};
        const iscore::sp<ModelDefaultDurationParameter> DefaultDuration{QStringLiteral("Skin/defaultDuration"), TimeValue::fromMsecs(15000)};

        static auto list() {
            return std::tie(Skin, GraphicZoom, SlotHeight, DefaultDuration);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx)
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

QString Model::getSkin() const
{
    return m_Skin;
}

void Model::setSkin(const QString& skin)
{
    if (m_Skin == skin)
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

    m_Skin = skin;

    QSettings s;
    s.setValue(Parameters::Skin.key, m_Skin);
    emit SkinChanged(skin);
}

ISCORE_SETTINGS_PARAMETER_CPP(double, Model, GraphicZoom)
ISCORE_SETTINGS_PARAMETER_CPP(qreal, Model, SlotHeight)
ISCORE_SETTINGS_PARAMETER_CPP(TimeValue, Model, DefaultDuration)

}
}
