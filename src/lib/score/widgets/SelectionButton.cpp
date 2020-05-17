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
    : QPushButton{text, parent}, m_dispatcher{disp}
{
  auto icon = makeIcons(
      QStringLiteral(":/icons/next_on.png"),
      QStringLiteral(":/icons/next_off.png"),
      QStringLiteral(":/icons/next_disabled.png"));

  setObjectName("SelectionButton");
  setIcon(icon);

#ifndef QT_NO_STYLE_STYLESHEET
  setStyleSheet(
      "margin: 5px;"
      "margin-left: 10px;"
      "text-align: left;"
      "text-decoration: underline;"
      "border: none;");
#endif
  setFlat(true);

  connect(this, &QPushButton::clicked, this, [=]() { m_dispatcher.setAndCommit(target); });
}
