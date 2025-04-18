// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSettingsView.hpp"

#include <score/widgets/FormWidget.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QFormLayout>

namespace Curve
{
namespace Settings
{

View::View()
{
  m_widg = new score::FormWidget{tr("Recording")};
  auto lay = m_widg->layout();

  {
    m_sb = new QDoubleSpinBox;

    m_sb->setMinimum(1);
    m_sb->setMaximum(10000);
    score::setHelp(m_sb, tr("Higher value is closer to the original data."));
    connect(
        m_sb, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
        &View::simplificationRatioChanged);

    lay->addRow(tr("Simplification Ratio"), m_sb);
  }

  {
    m_simpl = new QCheckBox{tr("Simplify")};
    score::setHelp(m_simpl, tr("Enable simplification of recorded data."));

    connect(m_simpl, SignalUtils::QCheckBox_checkStateChanged(), this, [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          simplifyChanged(false);
          break;
        case Qt::Checked:
          simplifyChanged(true);
          break;
        default:
          break;
      }
    });

    lay->addRow(m_simpl);
  }

  {
    m_mode = new QCheckBox{tr("Ramp to new value")};
    score::setHelp(
        m_simpl, tr("When simplifying, interpolate between recorded events instead of "
                    "maintaining the last received value until a change occurs."));

    connect(m_mode, SignalUtils::QCheckBox_checkStateChanged(), this, [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          modeChanged(Mode::Parameter);
          break;
        case Qt::Checked:
          modeChanged(Mode::Message);
          break;
        default:
          break;
      }
    });

    lay->addRow(m_mode);
  }

  {
    m_playWhileRecording = new QCheckBox{tr("Play while recording")};
    score::setHelp(
        m_simpl, tr("Enable playback of the rest of the score while recording."));

    connect(m_playWhileRecording, SignalUtils::QCheckBox_checkStateChanged(), this, [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          playWhileRecordingChanged(false);
          break;
        case Qt::Checked:
          playWhileRecordingChanged(true);
          break;
        default:
          break;
      }
    });

    lay->addRow(m_playWhileRecording);
  }
}

void View::setSimplificationRatio(int val)
{
  if(val != m_sb->value())
    m_sb->setValue(val);
}

void View::setSimplify(bool val)
{
  switch(m_simpl->checkState())
  {
    case Qt::Unchecked:
      if(val)
        m_simpl->setChecked(true);
      break;
    case Qt::Checked:
      if(!val)
        m_simpl->setChecked(false);
      break;
    default:
      break;
  }
}

void View::setMode(Mode val)
{
  switch(m_mode->checkState())
  {
    case Qt::Unchecked:
      if(val == Mode::Message)
        m_mode->setChecked(true);
      break;
    case Qt::Checked:
      if(val == Mode::Parameter)
        m_mode->setChecked(false);
      break;
    default:
      break;
  }
}

void View::setPlayWhileRecording(bool b)
{
  switch(m_playWhileRecording->checkState())
  {
    case Qt::Unchecked:
      if(b)
        m_playWhileRecording->setChecked(true);
      break;
    case Qt::Checked:
      if(!b)
        m_playWhileRecording->setChecked(false);
      break;
    default:
      break;
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
