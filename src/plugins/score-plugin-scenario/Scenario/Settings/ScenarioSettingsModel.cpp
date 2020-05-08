// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsModel.hpp"

#include <score/model/Skin.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/application/ApplicationContext.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>
#include <QDebug>
namespace Scenario
{
namespace Settings
{
namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Skin){QStringLiteral("Skin/Skin"), "Default"};
SETTINGS_PARAMETER_IMPL(DefaultEditor){QStringLiteral("Skin/DefaultEditor"),
                                       ""};
SETTINGS_PARAMETER_IMPL(GraphicZoom){QStringLiteral("Skin/Zoom"), 1};
SETTINGS_PARAMETER_IMPL(SlotHeight){QStringLiteral("Skin/slotHeight"), 200};
SETTINGS_PARAMETER_IMPL(DefaultDuration){
    QStringLiteral("Skin/defaultDuration"),
    TimeVal::fromMsecs(15000)};
SETTINGS_PARAMETER_IMPL(SnapshotOnCreate){
    QStringLiteral("Scenario/SnapshotOnCreate"),
    false};
SETTINGS_PARAMETER_IMPL(AutoSequence){QStringLiteral("Scenario/AutoSequence"),
                                      false};
SETTINGS_PARAMETER_IMPL(TimeBar){QStringLiteral("Scenario/TimeBar"), true};
SETTINGS_PARAMETER_IMPL(MeasureBars){QStringLiteral("Scenario/MeasureBars"), true};
SETTINGS_PARAMETER_IMPL(MagneticMeasures){QStringLiteral("Scenario/MagneticMeasures"), true};

static auto list()
{
  return std::tie(
      Skin,
      DefaultEditor,
      GraphicZoom,
      SlotHeight,
      DefaultDuration,
      SnapshotOnCreate,
      AutoSequence,
      TimeBar);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  if(m_DefaultDuration > TimeVal::fromMsecs(10000000))
    setDefaultDuration(TimeVal::fromMsecs(100000));
  setDefaultDuration(TimeVal::fromMsecs(100000));
}

QString Model::getSkin() const
{
  return m_Skin;
}

void Model::setSkin(const QString& skin)
{
  if (m_Skin == skin)
    return;
  if(!score::AppContext().applicationSettings.gui)
    return;

  QFile f(skin);
  if (skin.isEmpty() || skin == QStringLiteral("Default"))
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

QString Model::getDefaultEditor() const
{
  return m_DefaultEditor;
}

void Model::setDefaultEditor(QString val)
{
  if (val == m_DefaultEditor)
    return;

  m_DefaultEditor = val;

  QSettings s;
  s.setValue(
      Parameters::DefaultEditor.key, QVariant::fromValue(m_DefaultEditor));
  DefaultEditorChanged(val);
}

TimeVal Model::getDefaultDuration() const
{
  return m_DefaultDuration;
}

void Model::setDefaultDuration(TimeVal val)
{
  val = std::max(val, TimeVal{std::chrono::milliseconds{100}});

  if (val == m_DefaultDuration)
    return;

  m_DefaultDuration = val;

  QSettings s;
  s.setValue(
      Parameters::DefaultDuration.key, QVariant::fromValue(m_DefaultDuration));

  DefaultDurationChanged(val);
}

SCORE_SETTINGS_PARAMETER_CPP(double, Model, GraphicZoom)
SCORE_SETTINGS_PARAMETER_CPP(qreal, Model, SlotHeight)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, SnapshotOnCreate)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoSequence)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, TimeBar)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MeasureBars)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MagneticMeasures)
}
}
