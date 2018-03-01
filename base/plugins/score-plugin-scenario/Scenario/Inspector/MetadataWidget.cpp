// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetadataWidget.hpp"

#include <score/model/ModelMetadata.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/CommentEdit.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>

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
auto colorPalette()
  -> color_widgets::ColorPaletteModel&
{
  using namespace color_widgets;
  static ColorPaletteModel p;
  auto& skin = score::Skin::instance();
  ColorPalette palette1;
  palette1.setColors(skin.getColors());
  palette1.setName("Choose a color");
  p.addPalette(palette1, false);
  QObject::connect(&skin, &score::Skin::changed, [] {
    p.removePalette(0, false);

    ColorPalette palette1;
    palette1.setColors(score::Skin::instance().getColors());
    palette1.setName("Choose a color");
    p.addPalette(palette1, false);
  });
  return p;
}
MetadataWidget::MetadataWidget(
    const score::ModelMetadata& metadata,
    const score::CommandStackFacade& m,
    const QObject* docObject,
    QWidget* parent)
    : QWidget(parent)
    , m_metadata{metadata}
    , m_commandDispatcher{m}
    , m_metadataLayout{this}
    , m_descriptionWidget{this}
    , m_descriptionLay{&m_descriptionWidget}
    , m_scriptingNameLine{metadata.getName(), this}
    , m_labelLine{metadata.getLabel(), this}
    , m_comments{metadata.getComment(), this}
    , m_colorButton{this}
    , m_cmtBtn{this}
    , m_meta{metadata.getExtendedMetadata(), this}
{
  // main
  m_metadataLayout.setSizeConstraint(QLayout::SetMinimumSize);
  m_metadataLayout.addLayout(&m_headerLay);
  m_headerLay.addLayout(&m_btnLay);

  // Name(s)
  m_descriptionLay.addRow(tr("Name"), &m_scriptingNameLine);
  m_descriptionLay.addRow(tr("Label"), &m_labelLine);

  m_descriptionWidget.setObjectName("Description");

  // color
  m_colorButton.setArrowType(Qt::NoArrow);
  m_colorButton.setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_colorButton.setPopupMode(QToolButton::InstantPopup);

  // comments
  m_comments.setVisible(false);

  m_cmtBtn.setArrowType(Qt::RightArrow);
  m_cmtBtn.setIconSize({4,4});
  m_cmtLay.addWidget(&m_cmtBtn);
  m_cmtLabel = new TextLabel{tr("properties & comments"), this};
  m_cmtLay.addWidget(m_cmtLabel);

  m_meta.setVisible(false);

  m_btnLay.addWidget(&m_colorButton);
  m_btnLay.addLayout(&m_cmtLay);

  m_headerLay.addWidget(&m_descriptionWidget);

  m_metadataLayout.addWidget(&m_meta);
  m_metadataLayout.addWidget(&m_comments);

  con(m_cmtBtn, &QToolButton::released, this, [&]() {
    m_cmtExpanded = !m_cmtExpanded;
    m_comments.setVisible(m_cmtExpanded);
    m_meta.setVisible(m_cmtExpanded);
    if (m_cmtExpanded)
      m_cmtBtn.setArrowType(Qt::DownArrow);
    else
      m_cmtBtn.setArrowType(Qt::RightArrow);
  });

  con(m_scriptingNameLine, &QLineEdit::editingFinished, [=]() {
    scriptingNameChanged(m_scriptingNameLine.text());
  });

  con(m_labelLine, &QLineEdit::editingFinished, [=]() {
    labelChanged(m_labelLine.text());
  });

  con(m_comments, &CommentEdit::editingFinished, [=]() {
    commentsChanged(m_comments.toPlainText());
  });

  con(
      m_meta, &ExtendedMetadataWidget::dataChanged, this,
      [=]() { extendedMetadataChanged(m_meta.currentMap()); },
      Qt::QueuedConnection);

  {
    using namespace color_widgets;
    static auto& palette = colorPalette();

    auto palette_widget = new ColorPaletteWidget{this};

    palette_widget->setModel(&palette);
    palette_widget->setReadOnly(true);

    connect(
        palette_widget, static_cast<void (ColorPaletteWidget::*)(int)>(
                            &ColorPaletteWidget::currentColorChanged),
        this, [=](int idx) {
          auto colors = palette.palette(0).colors();

          if (idx >= 0 && idx < colors.size())
          {
            auto col_1 = colors.at(idx).second;
            auto col = score::ColorRef::ColorFromString(col_1);
            if (col)
              colorChanged(*col);
          }

        });

    auto colorMenu = new QMenu{this};
    auto act = new QWidgetAction(colorMenu);
    act->setDefaultWidget(palette_widget);
    colorMenu->insertAction(nullptr, act);
    m_colorButton.setMenu(colorMenu);
    m_colorButton.setMaximumSize(
        QSize(1.5 * m_colorIconSize, 1.5 * m_colorIconSize));

    m_colorButtonPixmap = QPixmap(m_colorIconSize, m_colorIconSize);
    m_colorButtonPixmap.fill(metadata.getColor().getBrush().color());
    m_colorButton.setIcon(QIcon(m_colorButtonPixmap));
    m_colorButton.setIconSize(QSize(m_colorIconSize, m_colorIconSize));
  }

  con(metadata, &score::ModelMetadata::metadataChanged, this,
      &MetadataWidget::updateAsked);
  updateAsked();
}

MetadataWidget::~MetadataWidget()
{
}

QString MetadataWidget::scriptingName() const
{
  return m_scriptingNameLine.text();
}

void MetadataWidget::setScriptingName(QString arg)
{
  if (m_scriptingNameLine.text() == arg)
  {
    return;
  }

  m_scriptingNameLine.setText(arg);
  scriptingNameChanged(arg);
}

void MetadataWidget::updateAsked()
{
  m_scriptingNameLine.setText(m_metadata.getName());
  m_labelLine.setText(m_metadata.getLabel());
  m_comments.setText(m_metadata.getComment());
  m_meta.update(m_metadata.getExtendedMetadata());

  m_colorButtonPixmap.fill(m_metadata.getColor().getBrush().color());
  m_colorButton.setIcon(QIcon(m_colorButtonPixmap));

  // m_currentColor = newColor;
}
}
