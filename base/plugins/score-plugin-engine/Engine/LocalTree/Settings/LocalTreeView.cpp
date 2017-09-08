// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalTreeView.hpp"
#include <QCheckBox>
#include <QFormLayout>
#include <score/widgets/SignalUtils.hpp>

namespace Engine
{
namespace LocalTree
{
namespace Settings
{

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  m_cb = new QCheckBox;
  lay->addRow(tr("Enable local tree"), m_cb);

  connect(m_cb, &QCheckBox::stateChanged, this, &View::localTreeChanged);
}

void View::setLocalTree(bool val)
{
  if (val != m_cb->checkState())
    m_cb->setCheckState(val ? Qt::Checked : Qt::Unchecked);
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
}
