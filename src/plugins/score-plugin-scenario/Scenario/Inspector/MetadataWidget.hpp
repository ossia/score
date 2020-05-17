#pragma once

#include <Inspector/InspectorLayout.hpp>
#include <Scenario/Commands/Metadata/ChangeElementColor.hpp>
#include <Scenario/Commands/Metadata/ChangeElementComments.hpp>
#include <Scenario/Commands/Metadata/ChangeElementLabel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/Metadata/SetExtendedMetadata.hpp>
#include <Scenario/Inspector/CommentEdit.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QLineEdit>
#include <QPixmap>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <verdigris>

namespace score
{
class ModelMetadata;
}

class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QToolButton;

namespace color_widgets
{
class ColorPaletteModel;
class Swatch;
}

namespace Scenario
{
class ExtendedMetadataWidget;
class CommentEdit;

// TODO move me in Process
class MetadataWidget final : public QWidget
{
  W_OBJECT(MetadataWidget)

public:
  explicit MetadataWidget(
      const score::ModelMetadata& metadata,
      const score::CommandStackFacade& m,
      const QObject* docObject,
      QWidget* parent = nullptr);

  ~MetadataWidget();

  template <typename T>
  void setupConnections(const T& model)
  {
    using namespace Scenario::Command;
    using namespace score::IDocument;
    connect(this, &MetadataWidget::labelChanged, [&](const QString& newLabel) {
      if (newLabel != model.metadata().getLabel())
        m_commandDispatcher.submit(new ChangeElementLabel<T>{model, newLabel});
    });

    connect(this, &MetadataWidget::commentsChanged, [&](const QString& newComments) {
      if (newComments != model.metadata().getComment())
        m_commandDispatcher.submit(new ChangeElementComments<T>{model, newComments});
    });

    connect(this, &MetadataWidget::colorChanged, [&](score::ColorRef newColor) {
      if (newColor != model.metadata().getColor())
        m_commandDispatcher.submit(new ChangeElementColor<T>{model, newColor});
    });

    /*
    connect(
        this, &MetadataWidget::extendedMetadataChanged,
        [&](const QVariantMap& newM) {
          if (newM != model.metadata().getExtendedMetadata())
            m_commandDispatcher.submit(
                new SetExtendedMetadata<T>{model, newM});
        });
    */
  }

  void updateAsked();

public:
  void labelChanged(QString arg) W_SIGNAL(labelChanged, arg);
  void commentsChanged(QString arg) W_SIGNAL(commentsChanged, arg);
  void colorChanged(score::ColorRef arg) W_SIGNAL(colorChanged, arg);
  void extendedMetadataChanged(const QVariantMap& arg) W_SIGNAL(extendedMetadataChanged, arg);

private:
  static const constexpr int m_colorIconSize{21};

  const score::ModelMetadata& m_metadata;
  CommandDispatcher<> m_commandDispatcher;

  Inspector::VBoxLayout m_metadataLayout;
  QLineEdit m_labelLine;
  CommentEdit m_comments;
  color_widgets::Swatch* m_palette_widget;
  QPixmap m_colorButtonPixmap{4 * m_colorIconSize / 3, 4 * m_colorIconSize / 3};
};
}
