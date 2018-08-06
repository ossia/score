// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorSectionWidget.hpp"

#include <QAction>
#include <QLayoutItem>
#include <QMenu>
#include <qnamespace.h>
#include <score/tools/Todo.hpp>
#include <score/widgets/SetIcons.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Inspector::InspectorSectionWidget)
namespace Inspector
{
MenuButton::MenuButton(QWidget* parent)
    : QPushButton{QStringLiteral("o"), parent}
{
  setFlat(true);
  setObjectName(QStringLiteral("SettingsMenu"));
  auto icon = makeIcons(QStringLiteral(":/icons/gear_on.png"),
                        QStringLiteral(":/icons/gear_off.png"),
                        QStringLiteral(":/icons/gear_disabled.png"));
  setIcon(icon);
  setIconSize(QSize(16, 16));
}

InspectorSectionWidget::InspectorSectionWidget(bool editable, QWidget* parent)
    : QWidget(parent)
    , m_generalLayout{this}
    , m_title{this}
    , m_titleLayout{&m_title}
    , m_unfoldBtn{&m_title}
    , m_buttonTitle{&m_title}
    , m_sectionTitle{&m_title}
    , m_menuBtn{&m_title}
{
  // HEADER : arrow button and name
  this->setContentsMargins(0, 0, 0, 0);
  m_title.setContentsMargins(0, 0, 0, 0);
  m_unfoldBtn.setIconSize({4, 4});

  m_buttonTitle.setObjectName(QStringLiteral("ButtonTitle"));
  m_buttonTitle.setText(QStringLiteral("section name"));

  m_sectionTitle.setObjectName("SectionTitle");
  con(m_sectionTitle, &QLineEdit::editingFinished, this,
      [=]() { nameChanged(m_sectionTitle.text()); });
  if (editable)
    m_buttonTitle.hide();
  else
    m_sectionTitle.hide();
  m_sectionTitle.setReadOnly(true);

  m_menuBtn.setObjectName(QStringLiteral("SettingsMenu"));
  m_menuBtn.setHidden(true);
  m_menuBtn.setFlat(true);
  m_menu = new QMenu{&m_menuBtn};
  m_menuBtn.setMenu(m_menu);

  m_titleLayout.addWidget(&m_unfoldBtn);
  m_titleLayout.addWidget(&m_sectionTitle);
  m_titleLayout.addWidget(&m_buttonTitle);
  m_titleLayout.addStretch(1);
  m_titleLayout.addWidget(&m_menuBtn);

  // GENERAL
  m_generalLayout.addWidget(&m_title);

  con(m_unfoldBtn, &QAbstractButton::released, this,
      [&] { this->expand(!m_isUnfolded); });
  con(m_buttonTitle, &QAbstractButton::clicked, this,
      [&] { this->expand(!m_isUnfolded); });

  // INIT
  m_isUnfolded = true;
  m_unfoldBtn.setArrowType(Qt::DownArrow);
  renameSection(QStringLiteral("Section Name"));
}

InspectorSectionWidget::InspectorSectionWidget(
    QString name, bool editable, QWidget* parent)
    : InspectorSectionWidget(editable, parent)
{
  renameSection(name);
  setObjectName(std::move(name));
}

InspectorSectionWidget::~InspectorSectionWidget() = default;

QString InspectorSectionWidget::name() const
{
  return m_sectionTitle.text();
}

void InspectorSectionWidget::expand(bool b)
{
  if (m_isUnfolded == b)
    return;
  else
    m_isUnfolded = b;

  for (int i = m_generalLayout.count() - 1; i >= 1; i--)
  {
    if (auto widg = m_generalLayout.itemAt(i)->widget())
      widg->setVisible(m_isUnfolded);
  }

  if (m_isUnfolded)
  {
    m_unfoldBtn.setArrowType(Qt::DownArrow);
  }
  else
  {
    m_unfoldBtn.setArrowType(Qt::RightArrow);
  }
}

void InspectorSectionWidget::renameSection(QString newName)
{
  m_sectionTitle.setText(newName);
  m_buttonTitle.setText(newName);
}

void InspectorSectionWidget::addContent(QWidget* newWidget)
{
  m_generalLayout.addWidget(newWidget);
}

void InspectorSectionWidget::removeContent(QWidget* toRemove)
{
  m_generalLayout.removeWidget(toRemove);
  delete toRemove;
}

void InspectorSectionWidget::removeAll()
{
  while (QLayoutItem* item = m_generalLayout.takeAt(1))
  {
    if (QWidget* wid = item->widget())
    {
      delete wid;
    }

    delete item;
  }
}

void InspectorSectionWidget::showMenu(bool b)
{
  m_menuBtn.setHidden(!b);
}
}
