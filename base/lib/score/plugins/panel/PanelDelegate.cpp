// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PanelDelegate.hpp"

namespace score
{

PanelStatus::PanelStatus(
    bool isShown,
    Qt::DockWidgetArea d,
    int prio,
    QString name,
    const QKeySequence& sc)
    : shown{isShown}
    , dock{d}
    , priority{prio}
    , prettyName{std::move(name)}
    , shortcut(sc)
{
}

PanelDelegate::PanelDelegate(const GUIApplicationContext& ctx) : m_context{ctx}
{
}

PanelDelegate::~PanelDelegate() = default;

void PanelDelegate::setModel(const DocumentContext& model)
{
  auto old = m_model;
  m_model = &model;
  on_modelChanged(old, m_model);
}

void PanelDelegate::setModel(none_t)
{
  auto old = m_model;
  m_model = nullptr;
  on_modelChanged(old, m_model);
}

MaybeDocument PanelDelegate::document() const
{
  return m_model;
}

const GUIApplicationContext& PanelDelegate::context() const
{
  return m_context;
}

void PanelDelegate::setNewSelection(const Selection& s)
{
}

void PanelDelegate::on_modelChanged(MaybeDocument oldm, MaybeDocument newm)
{
}
}
