#include <Curve/ApplicationPlugin.hpp>

#include <QKeyEvent>

namespace Curve
{

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  if (!ctx.mainWindow)
    return;

  // Setup the actions
  m_actions = new QActionGroup{this};
  m_actions->setExclusive(true);

  m_shiftact = new QAction{m_actions};
  m_shiftact->setCheckable(true);
  m_actions->addAction(m_shiftact);

  m_ctrlact = new QAction{m_actions};
  m_ctrlact->setCheckable(true);
  m_actions->addAction(m_ctrlact);

  m_altact = new QAction{m_actions};
  m_altact->setCheckable(true);
  m_altact->setChecked(false);
  m_actions->addAction(m_altact);

  m_actions->setEnabled(true);

  connect(m_altact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::CreatePen);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });
  connect(m_shiftact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::SetSegment);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });
  connect(m_ctrlact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::Create);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });
}

void ApplicationPlugin::on_keyPressEvent(QKeyEvent& event)
{
  auto key = event.key();
  if (key == Qt::Key_Shift)
  {
    m_shiftact->setChecked(true);
  }
  else if (key == Qt::Key_Control && editionSettings().tool() != Curve::Tool::SetSegment)
  {
    m_ctrlact->setChecked(true);
  }
  else if (key == Qt::Key_Alt && editionSettings().tool() != Curve::Tool::SetSegment)
  {
    m_altact->setChecked(!m_altact->isChecked());
  }
}

void ApplicationPlugin::on_keyReleaseEvent(QKeyEvent& event)
{
  auto key = event.key();
  if (key == Qt::Key_Shift)
  {
    m_shiftact->setChecked(false);
  }
  else if (key == Qt::Key_Control && editionSettings().tool() != Curve::Tool::SetSegment)
  {
    m_ctrlact->setChecked(false);
  }
  else if (key == Qt::Key_Alt && editionSettings().tool() != Curve::Tool::SetSegment)
  {
    m_altact->setChecked(!m_altact->isChecked());
  }
}

ApplicationPlugin::~ApplicationPlugin() { }

}
