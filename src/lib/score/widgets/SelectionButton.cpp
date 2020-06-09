// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionButton.hpp"

#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>

SelectionButton::SelectionButton(
    const QString& text,
    Selection target,
    score::SelectionDispatcher& disp,
    QWidget* parent)
    : QToolButton{parent}
    , m_dispatcher{disp}
{
  setText(text);
  auto icon = makeIcons(
      QStringLiteral(":/icons/next_on.png"),
      QStringLiteral(":/icons/next_off.png"),
      QStringLiteral(":/icons/next_disabled.png"));

  setObjectName("SelectionButton");
  setIcon(icon);

  connect(this, &QToolButton::clicked, this, [=]() { m_dispatcher.setAndCommit(target); });
}
