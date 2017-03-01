#include "MetadataWidget.hpp"

#include <iscore/model/ModelMetadata.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/CommentEdit.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QtColorWidgets/color_palette_widget.hpp>

#include <QBoxLayout>
#include <QColorDialog>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSize>
#include <QToolButton>
#include <QWidgetAction>

namespace Scenario
{
MetadataWidget::MetadataWidget(
    const iscore::ModelMetadata& metadata,
    const iscore::CommandStackFacade& m,
    const QObject* docObject,
    QWidget* parent)
    : QWidget(parent), m_metadata{metadata}, m_commandDispatcher{m}
{
  // main
  auto metadataLayout = new iscore::MarginLess<QVBoxLayout>{this};
  metadataLayout->setSizeConstraint(QLayout::SetMinimumSize);
  auto headerLay = new iscore::MarginLess<QHBoxLayout>{};
  metadataLayout->addLayout(headerLay);
  auto btnLay = new iscore::MarginLess<QVBoxLayout>{};
  headerLay->addLayout(btnLay);

  // Name(s)
  auto descriptionWidget = new QWidget{this};
  auto descriptionLay = new iscore::MarginLess<QFormLayout>{descriptionWidget};

  m_scriptingNameLine = new QLineEdit{metadata.getName(), this};
  m_labelLine = new QLineEdit{metadata.getLabel(), this};

  descriptionLay->addRow("Name", m_scriptingNameLine);
  descriptionLay->addRow("Label", m_labelLine);

  descriptionWidget->setObjectName("Description");

  // color
  m_colorButton = new QToolButton{this};
  m_colorButton->setArrowType(Qt::NoArrow);
  m_colorButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_colorButton->setPopupMode(QToolButton::InstantPopup);

  // comments
  m_comments = new CommentEdit{metadata.getComment(), this};
  m_comments->setVisible(false);

  m_cmtBtn = new QToolButton{this};
  m_cmtBtn->setArrowType(Qt::RightArrow);

  m_meta = new ExtendedMetadataWidget{metadata.getExtendedMetadata(), this};
  m_meta->setVisible(false);

  btnLay->addWidget(m_colorButton);
  btnLay->addWidget(m_cmtBtn);

  headerLay->addWidget(descriptionWidget);

  metadataLayout->addWidget(m_meta);
  metadataLayout->addWidget(m_comments);

  connect(m_cmtBtn, &QToolButton::released, this, [&]() {
    m_cmtExpanded = !m_cmtExpanded;
    m_comments->setVisible(m_cmtExpanded);
    m_meta->setVisible(m_cmtExpanded);
    if (m_cmtExpanded)
      m_cmtBtn->setArrowType(Qt::DownArrow);
    else
      m_cmtBtn->setArrowType(Qt::RightArrow);
  });

  connect(m_scriptingNameLine, &QLineEdit::editingFinished, [=]() {
    emit scriptingNameChanged(m_scriptingNameLine->text());
  });

  connect(m_labelLine, &QLineEdit::editingFinished, [=]() {
    emit labelChanged(m_labelLine->text());
  });

  connect(m_comments, &CommentEdit::editingFinished, [=]() {
    emit commentsChanged(m_comments->toPlainText());
  });

  connect(
      m_meta, &ExtendedMetadataWidget::dataChanged, this,
      [=]() { emit extendedMetadataChanged(m_meta->currentMap()); },
      Qt::QueuedConnection);

  {
    using namespace color_widgets;

    auto palette_widget = new ColorPaletteWidget{this};
    delete m_palette;
    m_palette = new ColorPaletteModel{};
    ColorPalette palette1;
    palette1.setColors(iscore::Skin::instance().getColors());
    palette1.setName("Choose a color");
    m_palette->addPalette(palette1, false);

    palette_widget->setModel(m_palette);
    palette_widget->setReadOnly(true);

    connect(
        palette_widget, static_cast<void (ColorPaletteWidget::*)(int)>(
                            &ColorPaletteWidget::currentColorChanged),
        this, [=](int idx) {
          auto colors = m_palette->palette(0).colors();

          if (idx >= 0 && idx < colors.size())
          {
            auto col_1 = colors.at(idx).second;
            auto col = iscore::ColorRef::ColorFromString(col_1);
            if (col)
              emit colorChanged(*col);
          }

        });

    auto colorMenu = new QMenu{this};
    auto act = new QWidgetAction(colorMenu);
    act->setDefaultWidget(palette_widget);
    colorMenu->insertAction(nullptr, act);
    m_colorButton->setMenu(colorMenu);
    m_colorButton->setMaximumSize(
        QSize(1.5 * m_colorIconSize, 1.5 * m_colorIconSize));

    m_colorButtonPixmap = QPixmap(m_colorIconSize, m_colorIconSize);
    m_colorButtonPixmap.fill(metadata.getColor().getColor().color());
    m_colorButton->setIcon(QIcon(m_colorButtonPixmap));
    m_colorButton->setIconSize(QSize(m_colorIconSize, m_colorIconSize));
  }

  con(metadata, &iscore::ModelMetadata::metadataChanged, this,
      &MetadataWidget::updateAsked);
  updateAsked();
}

MetadataWidget::~MetadataWidget()
{
  delete m_palette;
}

QString MetadataWidget::scriptingName() const
{
  return m_scriptingNameLine->text();
}

void MetadataWidget::setScriptingName(QString arg)
{
  if (m_scriptingNameLine->text() == arg)
  {
    return;
  }

  m_scriptingNameLine->setText(arg);
  emit scriptingNameChanged(arg);
}

void MetadataWidget::updateAsked()
{
  m_scriptingNameLine->setText(m_metadata.getName());
  m_labelLine->setText(m_metadata.getLabel());
  m_comments->setText(m_metadata.getComment());
  m_meta->update(m_metadata.getExtendedMetadata());

  m_colorButtonPixmap.fill(m_metadata.getColor().getColor().color());
  m_colorButton->setIcon(QIcon(m_colorButtonPixmap));

  // m_currentColor = newColor;
}
}
