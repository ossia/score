#include "ScenarioSettingsPresenter.hpp"
#include "ScenarioSettingsModel.hpp"
#include "ScenarioSettingsView.hpp"
#include <QApplication>
#include <QStyle>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/command/SettingsCommand.hpp>

namespace Scenario
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : iscore::SettingsDelegatePresenter{m, v, parent}
{
  con(v, &View::skinChanged, this, [&](const auto& val) {
    if (val != m.getSkin())
    {
      m_disp.submitCommand<SetModelSkin>(this->model(this), val);
    }
  });

  con(m, &Model::SkinChanged, &v, &View::setSkin);
  v.setSkin(m.getSkin());

  con(v, &View::zoomChanged, this, [&](auto val) {
    if (val != m.getGraphicZoom())
    {
      m_disp.submitCommand<SetModelGraphicZoom>(
          this->model(this), 0.01 * double(val));
    }
  });
  con(m, &Model::GraphicZoomChanged, this,
      [&](double z) { v.setZoom((100 * z)); });
  v.setZoom(m.getGraphicZoom() * 100);

  con(v, &View::slotHeightChanged, this, [&](int h) {
    if (h != m.getSlotHeight())
    {
      m_disp.submitCommand<SetModelSlotHeight>(this->model(this), qreal(h));
    }
  });
  con(m, &Model::SlotHeightChanged, this,
      [&](const qreal h) { v.setSlotHeight(h); });
  v.setSlotHeight(qreal(m.getSlotHeight()));

  con(v, &View::defaultDurationChanged, this, [&](const TimeVal& t) {
    if (t != m.getDefaultDuration())
    {
      m_disp.submitCommand<SetModelDefaultDuration>(this->model(this), t);
    }
  });
  con(m, &Model::DefaultDurationChanged, this,
      [&](const TimeVal& t) { v.setDefaultDuration(t); });
  v.setDefaultDuration(m.getDefaultDuration());

  con(v, &View::snapshotChanged, this, [&](bool b) {
    if (b != m.getSnapshotOnCreate())
      m_disp.submitCommand<SetModelSnapshotOnCreate>(this->model(this), b);
  });
  con(m, &Model::SnapshotOnCreateChanged, this,
      [&](bool b) { v.setSnapshot(b); });
  v.setSnapshot(m.getSnapshotOnCreate());

  con(v, &View::sequenceChanged, this, [&](bool b) {
    if (b != m.getAutoSequence())
      m_disp.submitCommand<SetModelAutoSequence>(this->model(this), b);
  });
  con(m, &Model::AutoSequenceChanged, this, [&](bool b) { v.setSequence(b); });
  v.setSequence(m.getAutoSequence());
}

QString Presenter::settingsName()
{
  return tr("Theme");
}

QIcon Presenter::settingsIcon()
{
  return QApplication::style()->standardIcon(QStyle::SP_DesktopIcon);
}
}
}
