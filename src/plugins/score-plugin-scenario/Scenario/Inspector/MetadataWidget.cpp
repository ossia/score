// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetadataWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Inspector/CommentEdit.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QLineEdit>
#include <QSize>
#include <QtColorWidgets/color_palette.hpp>
#include <QtColorWidgets/color_palette_model.hpp>
#include <QtColorWidgets/swatch.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::MetadataWidget)
W_OBJECT_IMPL(Scenario::CommentEdit)
namespace Scenario
{
auto colorPalette() -> color_widgets::ColorPaletteModel&
{
  using namespace color_widgets;
  static ColorPaletteModel p;
  auto& skin = score::Skin::instance();
  ColorPalette palette;
  palette.setColors(skin.getDefaultPaletteColors());

  p.addPalette(palette, false);

  QObject::connect(&skin, &score::Skin::changed, [] {
    p.removePalette(0, false);

    ColorPalette palette;
    palette.setColors(score::Skin::instance().getDefaultPaletteColors());
    p.addPalette(palette, false);
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
    , m_labelLine{metadata.getLabel(), this}
    , m_comments{metadata.getComment(), this}
{
  // main
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

  // Name(s)
  float margin = m_comments.document()->documentMargin() / 2.;

  m_labelLine.setTextMargins(margin, 0, margin, 0);
  m_labelLine.setPlaceholderText(tr("Label"));
  m_labelLine.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  m_metadataLayout.addWidget(&m_labelLine);
  con(metadata, &score::ModelMetadata::LabelChanged, this, [=](const auto& str) {
    m_labelLine.setText(str);
  });

  // comments
  m_comments.setMaximumHeight(50);
  m_comments.setPlaceholderText(tr("Comments"));
  con(metadata, &score::ModelMetadata::CommentChanged, this, [=](const auto& str) {
    m_comments.setText(str);
  });

  m_metadataLayout.addWidget(&m_comments);

  // color palette
  {
    using namespace color_widgets;
    static auto& color_palette = colorPalette();

    m_palette_widget = new Swatch(this);
    m_palette_widget->setPalette(color_palette.palette(0));
    m_palette_widget->setReadOnly(true);

    m_palette_widget->setColorSize(QSize(20, 20));
    m_palette_widget->setColorSizePolicy(Swatch::ColorSizePolicy::Fixed);
    m_palette_widget->setSelection(QPen(QColor(0, 0, 0), 2));
    m_palette_widget->setBorder(QPen(Qt::transparent));

    int forced_rows = 2;
    m_palette_widget->setForcedRows(forced_rows);
    // m_palette_widget->setMaximumWidth(
    //    20 * m_palette_widget->palette().count() / forced_rows);
    m_palette_widget->setMaximumHeight(20 * forced_rows);

    connect(m_palette_widget, &Swatch::selectedChanged, this, [=](int idx) {
      auto colors = color_palette.palette(0).colors();

      if (idx == colors.size() - 1)
      {
        const score::Brush& defaultBrush = Process::Style::instance().IntervalDefaultBackground();
        colorChanged(&defaultBrush);
      }
      else if (idx >= 0 && idx < colors.size())
      {
        auto col_1 = colors.at(idx).second;
        auto col = score::ColorRef::ColorFromString(col_1);
        if (col)
          colorChanged(*col);
      }
    });

    con(metadata, &score::ModelMetadata::ColorChanged, this, [=](const score::ColorRef& str) {
      auto palette = m_palette_widget->palette();
      auto color = str.getBrush().color();
      for (int i = 0; i < palette.count(); i++)
      {
        if (palette.colorAt(i) == color)
        {
          m_palette_widget->setSelected(i);
          break;
        }
      }
    });
    m_metadataLayout.addWidget(m_palette_widget);
  }

  con(m_labelLine, &QLineEdit::editingFinished, [=]() { labelChanged(m_labelLine.text()); });

  con(m_comments, &CommentEdit::editingFinished, [=]() {
    commentsChanged(m_comments.toPlainText());
  });

  con(metadata, &score::ModelMetadata::metadataChanged, this, &MetadataWidget::updateAsked);
  updateAsked();
}

MetadataWidget::~MetadataWidget() { }

void MetadataWidget::updateAsked()
{
  m_labelLine.setText(m_metadata.getLabel());
  m_comments.setText(m_metadata.getComment());

  const auto& palette = m_palette_widget->palette();
  auto color = m_metadata.getColor().getBrush().color();
  for (int i = 0; i < palette.count(); i++)
  {
    if (palette.colorAt(i) == color)
    {
      m_palette_widget->setSelected(i);
      break;
    }
  }
}
}
