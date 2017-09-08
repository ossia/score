// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <score/document/DocumentContext.hpp>

#include "InspectorWidgetBase.hpp"
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/widgets/MarginLess.hpp>

namespace Inspector
{
InspectorWidgetBase::InspectorWidgetBase(
    const IdentifiedObjectAbstract& inspectedObj,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : QWidget(parent)
    , m_inspectedObject{inspectedObj}
    , m_context{ctx}
    , m_commandDispatcher(new CommandDispatcher<>{ctx.commandStack})
    , m_selectionDispatcher(
          new score::SelectionDispatcher{ctx.selectionStack})

{
  m_layout = new QVBoxLayout;
  m_layout->setContentsMargins(0, 1, 0, 0);
  setLayout(m_layout);
  m_layout->setSpacing(0);

  // scroll Area
  auto scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
  scrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  auto scrollAreaContentWidget = new QWidget;
  m_scrollAreaLayout
      = new score::MarginLess<QVBoxLayout>{scrollAreaContentWidget};
  m_scrollAreaLayout->setSizeConstraint(QLayout::SetMinimumSize);
  scrollArea->setWidget(scrollAreaContentWidget);

  m_sections.push_back(scrollArea);

  updateSectionsView(m_layout, m_sections);
}

InspectorWidgetBase::~InspectorWidgetBase()
{
  delete m_commandDispatcher;
}

QString InspectorWidgetBase::tabName()
{
  return m_inspectedObject.objectName();
}

void InspectorWidgetBase::updateSectionsView(
    QVBoxLayout* layout, const std::vector<QWidget*>& contents)
{
  while (!layout->isEmpty())
  {
    auto item = layout->takeAt(0);

    if(auto widg = item->widget())
      delete widg;
    delete item;
  }

  for (auto section : contents)
  {
    layout->addWidget(section);
  }
}

void InspectorWidgetBase::updateAreaLayout(std::initializer_list<QWidget*> contents)
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

void InspectorWidgetBase::updateAreaLayout(const std::vector<QWidget*>& contents)
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
  m_sections.insert(m_sections.begin(), header);
  m_layout->insertWidget(0, header);
}

const IdentifiedObjectAbstract& InspectorWidgetBase::inspectedObject() const
{
  return m_inspectedObject;
}
}
