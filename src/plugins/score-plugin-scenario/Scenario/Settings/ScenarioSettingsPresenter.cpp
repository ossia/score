// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsPresenter.hpp"

#include "ScenarioSettingsModel.hpp"
#include "ScenarioSettingsView.hpp"

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::Settings::Model)
W_OBJECT_IMPL(Scenario::Settings::View)
namespace Scenario
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(DefaultEditor);
  SETTINGS_PRESENTER(Skin);
  SETTINGS_PRESENTER(SlotHeight);
  SETTINGS_PRESENTER(AutoSequence);
  SETTINGS_PRESENTER(TimeBar);
  SETTINGS_PRESENTER(MeasureBars);
  SETTINGS_PRESENTER(MagneticMeasures);
  SETTINGS_PRESENTER(DefaultDuration);

  con(v, &View::zoomChanged, this, [&](auto val) {
    if (val != m.getGraphicZoom())
    {
      m_disp.submit<SetModelGraphicZoom>(
          this->model(this), 0.01 * double(val));
    }
  });
  con(m, &Model::GraphicZoomChanged, this, [&](double z) {
    v.setZoom((100 * z));
  });
  v.setZoom(m.getGraphicZoom() * 100);
}

QString Presenter::settingsName()
{
  return tr("User interface");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(QStringLiteral(":/icons/settings_ui_on.png")
                   , QStringLiteral(":/icons/settings_ui_off.png")
                   , QStringLiteral(":/icons/settings_ui_off.png"));

}
}
}
