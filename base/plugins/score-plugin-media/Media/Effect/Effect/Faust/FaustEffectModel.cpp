#include "FaustEffectModel.hpp"
#include <Media/MediaStreamEngine/MediaApplicationPlugin.hpp>
namespace Media
{
namespace Effect
{

FaustEffectModel::FaustEffectModel(
        const QString& faustProgram,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
    setText(faustProgram);
    init();
}

FaustEffectModel::FaustEffectModel(
        const FaustEffectModel& source,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
    setText(source.text());
    init();
}

void FaustEffectModel::setText(const QString& txt)
{
    m_text = txt;
    reload();
}

void FaustEffectModel::init()
{
    // We have to reload the faust FX whenever
    // some soundcard settings changes
    auto& ctx = score::AppComponents().applicationPlugin<Media::MediaStreamEngine::ApplicationPlugin>();
    con(ctx, &MediaStreamEngine::ApplicationPlugin::audioEngineRestarting,
        this, [this] () {
        saveParams();
    });
    con(ctx, &MediaStreamEngine::ApplicationPlugin::audioEngineRestarted,
            this, [this] () {
        reload();
    });
}

void FaustEffectModel::reload()
{
    auto fx_text = m_text.toLocal8Bit();
    if(!fx_text.isEmpty())
        m_effect = MakeFaustMediaEffect(fx_text, "/usr/local/share/faust/", ""); // TODO compute the path to the "architecture" folder here

    if(m_effect)
    {
        auto json = GetJsonEffect(m_effect);
        QJsonParseError err;
        auto qjs = QJsonDocument::fromJson(json, &err);
        if(err.error == QJsonParseError::NoError)
        {
            metadata().setLabel(qjs.object()["name"].toString());
        }
        else
        {
            qDebug() << err.errorString();
        }

        restoreParams();
    }
    else
    {
        qDebug() << "could not load effect";
    }

    emit effectChanged();
}


}
}
