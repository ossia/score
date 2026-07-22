// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorWidgetBase.hpp"

#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QColor>
#include <QScrollArea>
#include <QWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Inspector::InspectorWidgetBase)
namespace Inspector
{
InspectorWidgetBase::InspectorWidgetBase(
    const IdentifiedObjectAbstract& inspectedObj, const score::DocumentContext& ctx,
    QWidget* parent, QString name)
    : QWidget(parent)
    , m_inspectedObject{inspectedObj}
    , m_context{ctx}
    , m_commandDispatcher(new CommandDispatcher<>{ctx.commandStack})
{
  m_layout = new VBoxLayout;

  setLayout(m_layout);

  const int typeLabelSize = 14;
  const int nameLabelSize = 16;

  if(name.contains('\n'))
  {
    const auto parts = name.split('\n');
    const QString typePart = parts.value(0).trimmed();
    const QString namePart = parts.value(1).trimmed();

    auto titleContainer = new QWidget(this);
    auto titleLay = new VBoxLayout{titleContainer};
    titleLay->setContentsMargins(0, 0, 0, 0);
    titleLay->setSpacing(2);

    auto typeLabel = new TextLabel{typePart, titleContainer};
    auto typeFont = typeLabel->font();
    typeFont.setBold(true);
    typeFont.setPixelSize(typeLabelSize);
    typeLabel->setFont(typeFont);
    // Type: default palette (no color override)
    titleLay->addWidget(typeLabel);

    auto nameLabel = new TextLabel{namePart, titleContainer};
    auto nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPixelSize(nameLabelSize);
    nameLabel->setFont(nameFont);
    auto namePal = nameLabel->palette();
    namePal.setColor(QPalette::WindowText, Qt::white);
    nameLabel->setPalette(namePal);
    titleLay->addWidget(nameLabel);

    m_label = typeLabel;
    m_sections.push_back(titleContainer);
  }
  else
  {
    m_label = new TextLabel{name, this};
    auto f = m_label->font();
    f.setBold(true);
    f.setPixelSize(nameLabelSize);
    m_label->setFont(f);
    m_sections.push_back(m_label);
  }

  auto titleSpacer = new QWidget(this);
  titleSpacer->setFixedHeight(8);
  titleSpacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_sections.push_back(titleSpacer);

  // scroll Area
  auto scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameStyle(QFrame::NoFrame);
  scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
  scrollArea->setSizePolicy(
      QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  auto scrollAreaContentWidget = new QWidget;
  m_scrollAreaLayout = new VBoxLayout{scrollAreaContentWidget};
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
    QVBoxLayout* layout, const std::vector<QWidget*>& contents)
{
  while(!layout->isEmpty())
  {
    auto item = layout->takeAt(0);

    if(auto widg = item->widget())
      delete widg;
    delete item;
  }

  for(auto section : contents)
  {
    layout->addWidget(section);
  }
}

void InspectorWidgetBase::updateAreaLayout(std::initializer_list<QWidget*> contents)
{
  while(!m_scrollAreaLayout->isEmpty())
  {
    auto item = m_scrollAreaLayout->takeAt(m_scrollAreaLayout->count() - 1);

    delete item->widget();
    delete item;
  }

  for(auto section : contents)
  {
    m_scrollAreaLayout->addWidget(section);
  }
  m_scrollAreaLayout->addStretch(1);
}

void InspectorWidgetBase::updateAreaLayout(const std::vector<QWidget*>& contents)
{
  while(!m_scrollAreaLayout->isEmpty())
  {
    auto item = m_scrollAreaLayout->takeAt(m_scrollAreaLayout->count() - 1);

    delete item->widget();
    delete item;
  }

  for(auto section : contents)
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
