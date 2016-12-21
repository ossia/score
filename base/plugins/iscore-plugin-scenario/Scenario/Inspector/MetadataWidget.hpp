#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include <Scenario/Commands/Metadata/ChangeElementColor.hpp>
#include <Scenario/Commands/Metadata/ChangeElementComments.hpp>
#include <Scenario/Commands/Metadata/ChangeElementLabel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/Metadata/SetExtendedMetadata.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <iscore/model/IdentifiedObject.hpp>

namespace iscore
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
}

namespace Scenario
{
class ExtendedMetadataWidget;
class CommentEdit;

// TODO move me in Process
class MetadataWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit MetadataWidget(
      const iscore::ModelMetadata& metadata,
      const iscore::CommandStackFacade& m,
      const QObject* docObject,
      QWidget* parent = nullptr);

  ~MetadataWidget();

  QString scriptingName() const;

  template <typename T>
  void setupConnections(const T& model)
  {
    using namespace Scenario::Command;
    using namespace iscore::IDocument;
    connect(
        this, &MetadataWidget::scriptingNameChanged,
        [&](const QString& newName) {
          if (newName != model.metadata().getName())
            m_commandDispatcher.submitCommand(
                new ChangeElementName<T>{path(model), newName});
        });

    connect(this, &MetadataWidget::labelChanged, [&](const QString& newLabel) {
      if (newLabel != model.metadata().getLabel())
        m_commandDispatcher.submitCommand(
            new ChangeElementLabel<T>{path(model), newLabel});
    });

    connect(
        this, &MetadataWidget::commentsChanged,
        [&](const QString& newComments) {
          if (newComments != model.metadata().getComment())
            m_commandDispatcher.submitCommand(
                new ChangeElementComments<T>{path(model), newComments});
        });

    connect(
        this, &MetadataWidget::colorChanged, [&](iscore::ColorRef newColor) {
          if (newColor != model.metadata().getColor())
            m_commandDispatcher.submitCommand(
                new ChangeElementColor<T>{path(model), newColor});
        });

    connect(
        this, &MetadataWidget::extendedMetadataChanged,
        [&](const QVariantMap& newM) {
          if (newM != model.metadata().getExtendedMetadata())
            m_commandDispatcher.submitCommand(
                new SetExtendedMetadata<T>{path(model), newM});
        });
  }

  void setScriptingName(QString arg);
  void updateAsked();

signals:
  void scriptingNameChanged(QString arg);
  void labelChanged(QString arg);
  void commentsChanged(QString arg);
  void colorChanged(iscore::ColorRef arg);
  void extendedMetadataChanged(const QVariantMap& arg);

private:
  const iscore::ModelMetadata& m_metadata;
  CommandDispatcher<> m_commandDispatcher;

  QLineEdit* m_scriptingNameLine{};
  QLineEdit* m_labelLine{};
  QToolButton* m_colorButton{};
  CommentEdit* m_comments{};
  ExtendedMetadataWidget* m_meta{};
  QPixmap m_colorButtonPixmap{4 * m_colorIconSize / 3,
                              4 * m_colorIconSize / 3};
  static const int m_colorIconSize{21};
  bool m_cmtExpanded{false};
  QToolButton* m_cmtBtn{};

  color_widgets::ColorPaletteModel* m_palette{};

  //        QString m_scriptingName;
};
}
