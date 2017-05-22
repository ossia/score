#pragma once

#include <QColor>
#include <QLineEdit>
#include <QPixmap>
#include <QString>
#include <QToolButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <Scenario/Inspector/CommentEdit.hpp>
#include <Scenario/Inspector/ExtendedMetadataWidget.hpp>

#include <Scenario/Commands/Metadata/ChangeElementColor.hpp>
#include <Scenario/Commands/Metadata/ChangeElementComments.hpp>
#include <Scenario/Commands/Metadata/ChangeElementLabel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/Metadata/SetExtendedMetadata.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>

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
          {
            if(!newName.isEmpty())
            {
              m_commandDispatcher.submitCommand(
                  new ChangeElementName<T>{path(model), newName});
            }
            else
            {
              m_commandDispatcher.submitCommand(
                  new ChangeElementName<T>{
                      path(model), QString("%1.0").arg(Metadata<PrettyName_k, T>::get())
                  });
            }
          }
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
  static const constexpr int m_colorIconSize{21};

  const iscore::ModelMetadata& m_metadata;
  CommandDispatcher<> m_commandDispatcher;

  iscore::MarginLess<QVBoxLayout> m_metadataLayout;
  iscore::MarginLess<QHBoxLayout> m_headerLay;
  iscore::MarginLess<QVBoxLayout> m_btnLay;
  QWidget m_descriptionWidget;
  iscore::MarginLess<QFormLayout> m_descriptionLay;
  QLineEdit m_scriptingNameLine;
  QLineEdit m_labelLine;
  CommentEdit m_comments;
  QToolButton m_colorButton;
  QToolButton m_cmtBtn;
  ExtendedMetadataWidget m_meta;
  QPixmap m_colorButtonPixmap{4 * m_colorIconSize / 3,
                              4 * m_colorIconSize / 3};

  bool m_cmtExpanded{false};
};
}
