// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorWidgetBase.hpp"

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
#include <Inspector/InspectorLayout.hpp>

#include <QScrollArea>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Inspector::InspectorWidgetBase)
namespace Inspector
{
InspectorWidgetBase::InspectorWidgetBase(
    const IdentifiedObjectAbstract& inspectedObj,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QString name)
    : QWidget(parent)
    , m_inspectedObject{inspectedObj}
    , m_context{ctx}
    , m_commandDispatcher(new CommandDispatcher<>{ctx.commandStack})
{
  m_layout = new VBoxLayout;
 // m_layout->setSpacing(5);

  setLayout(m_layout);

  m_label = new TextLabel{name, this};
  auto f = m_label->font();
  f.setBold(true);
  f.setPixelSize(18);
  m_label->setFont(f);
  m_sections.push_back(m_label);

  // scroll Area
  auto scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
  scrollArea->setSizePolicy(
      QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  auto scrollAreaContentWidget = new QWidget;
  m_scrollAreaLayout
      = new QVBoxLayout{scrollAreaContentWidget};
  m_scrollAreaLayout->setSizeConstraint(QLayout::SetMinimumSize);
  scrollArea->setWidget(scrollAreaContentWidget);

  m_sections.push_back(scrollArea);

  updateSectionsView(m_layout, m_sections);
}

InspectorWidgetBase::~InspectorWidgetBase()
{
  delete m_commandDispatcher;
}

void InspectorWidgetBase::updateSectionsView(
    QVBoxLayout* layout,
    const std::vector<QWidget*>& contents)
{
  while (!layout->isEmpty())
  {
    auto item = layout->takeAt(0);

    if (auto widg = item->widget())
      delete widg;
    delete item;
  }

  for (auto section : contents)
  {
    layout->addWidget(section);
  }
}

void InspectorWidgetBase::updateAreaLayout(
    std::initializer_list<QWidget*> contents)
{
  while (!m_scrollAreaLayout->isEmpty())
  {
    auto item = m_scrollAreaLayout->takeAt(m_scrollAreaLayout->count() - 1);

    delete item->widget();
    delete item;
  }

  for (auto section : contents)
  {
    m_scrollAreaLayout->addWidget(section);
  }
  m_scrollAreaLayout->addStretch(1);
}

void InspectorWidgetBase::updateAreaLayout(
    const std::vector<QWidget*>& contents)
{
  while (!m_scrollAreaLayout->isEmpty())
  {
    auto item = m_scrollAreaLayout->takeAt(m_scrollAreaLayout->count() - 1);

    delete item->widget();
    delete item;
  }

  for (auto section : contents)
  {
    m_scrollAreaLayout->addWidget(section);
  }
  m_scrollAreaLayout->addStretch(1);
}

void InspectorWidgetBase::addHeader(QWidget* header)
{
  QWidget* title = m_sections[0];
  m_sections[0] = header;
  m_sections.insert(m_sections.begin(), title);
  m_layout->insertWidget(1, header);
}

const IdentifiedObjectAbstract& InspectorWidgetBase::inspectedObject() const
{
  return m_inspectedObject;
}
}
