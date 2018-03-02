// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsModel.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <score/model/Skin.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
namespace Settings
{
namespace Parameters
{
const score::sp<ModelSkinParameter> Skin{QStringLiteral("Skin/Skin"),
                                          "Default"};
const score::sp<ModelGraphicZoomParameter> GraphicZoom{
    QStringLiteral("Skin/Zoom"), 1};
const score::sp<ModelSlotHeightParameter> SlotHeight{
    QStringLiteral("Skin/slotHeight"), 200};
const score::sp<ModelDefaultDurationParameter> DefaultDuration{
    QStringLiteral("Skin/defaultDuration"), TimeVal::fromMsecs(15000)};
const score::sp<ModelSnapshotOnCreateParameter> SnapshotOnCreate{
    QStringLiteral("Scenario/SnapshotOnCreate"), false};
const score::sp<ModelAutoSequenceParameter> AutoSequence{
    QStringLiteral("Scenario/AutoSequence"), true};
const score::sp<ModelTimeBarParameter> TimeBar{
  QStringLiteral("Scenario/TimeBar"), true};

static auto list()
{
  return std::tie(
      Skin, GraphicZoom, SlotHeight, DefaultDuration, SnapshotOnCreate,
      AutoSequence, TimeBar);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

QString Model::getSkin() const
{
  return m_Skin;
}

void Model::setSkin(const QString& skin)
{
  if (m_Skin == skin)
    return;

  QFile f(skin);
  if(skin.isEmpty() || skin == QStringLiteral("Default"))
      f.setFileName(":/DefaultSkin.json");
  else if (skin == QStringLiteral("Dark"))
      f.setFileName(":/DarkSkin.json");
  else if (skin == QStringLiteral("IEEE"))
      f.setFileName(":/IEEESkin.json");

  if (f.open(QFile::ReadOnly))
  {
    auto arr = f.readAll();
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(arr, &err);
    if (err.error)
    {
      qDebug() << "could not load skin : " << err.errorString() << err.offset;
    }
    else
    {
      auto obj = doc.object();
      score::Skin::instance().load(obj);
    }
  }
  else
  {
    qDebug() << "could not open" << f.fileName();
  }

  m_Skin = skin;

  QSettings s;
  s.setValue(Parameters::Skin.key, m_Skin);
  SkinChanged(skin);
}

SCORE_SETTINGS_PARAMETER_CPP(double, Model, GraphicZoom)
SCORE_SETTINGS_PARAMETER_CPP(qreal, Model, SlotHeight)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, SnapshotOnCreate)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoSequence)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, TimeBar)

  TimeVal Model::getDefaultDuration() const
  {
    return m_DefaultDuration;
  }

  void Model::setDefaultDuration(TimeVal val)
  {
    val = std::max(val, TimeVal{std::chrono::seconds{10}});
    if (val == m_DefaultDuration)
      return;

    m_DefaultDuration = val;

    QSettings s;
    s.setValue(Parameters::DefaultDuration.key, QVariant::fromValue(m_DefaultDuration));
    DefaultDurationChanged(val);
  }
}
}
