#include "FaustEffectModel.hpp"
#include <Media/MediaStreamEngine/MediaApplicationPlugin.hpp>
namespace Media
{
namespace Effect
{

struct FaustEditDialog : public QDialog
{
        const FaustEffectModel& m_effect;

        QPlainTextEdit* m_textedit{};
    public:
        FaustEditDialog(const FaustEffectModel& fx):
            m_effect{fx}
        {
            auto lay = new QVBoxLayout;
            this->setLayout(lay);

            m_textedit = new QPlainTextEdit{m_effect.text()};

            lay->addWidget(m_textedit);
            auto bbox = new QDialogButtonBox{
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
            lay->addWidget(bbox);
            connect(bbox, &QDialogButtonBox::accepted,
                    this, &QDialog::accept);
            connect(bbox, &QDialogButtonBox::rejected,
                    this, &QDialog::reject);
        }

        QString text() const
        {
            return m_textedit->document()->toPlainText();
        }
};

FaustEffectModel::FaustEffectModel(
        const QString& faustProgram,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
    setText(faustProgram);
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

}

void FaustEffectModel::showUI()
{
  FaustEditDialog edit{*faust};
  auto res = edit.exec();
  if(res)
  {
      m_dispatcher.submitCommand(new Commands::EditFaustEffect{*faust, edit.text()});
  }
}

void FaustEffectModel::hideUI()
{

}


}
}
