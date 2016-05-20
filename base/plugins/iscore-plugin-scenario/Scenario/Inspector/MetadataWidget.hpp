#pragma once

#include <Scenario/Commands/Metadata/ChangeElementColor.hpp>
#include <Scenario/Commands/Metadata/ChangeElementComments.hpp>
#include <Scenario/Commands/Metadata/ChangeElementLabel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <QColor>
#include <QPixmap>
#include <QString>
#include <QWidget>

#include <iscore/tools/IdentifiedObject.hpp>

class ModelMetadata;
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

class CommentEdit;

// TODO move me in Process
class MetadataWidget final : public QWidget
{
        Q_OBJECT

    public:
        explicit MetadataWidget(
                const ModelMetadata* metadata,
                CommandDispatcher<>* m,
                const QObject* docObject,
                QWidget* parent = nullptr);

        QString scriptingName() const;

        template<typename T>
        void setupConnections(const T& model)
        {
            using namespace Scenario::Command;
            using namespace iscore::IDocument;
            connect(this, &MetadataWidget::scriptingNameChanged,
                    [&](const QString& newName)
            {
                if(newName != model.metadata.name())
                    m_commandDispatcher->submitCommand(new ChangeElementName<T>{path(model), newName});
            });

            connect(this, &MetadataWidget::labelChanged,
                    [&](const QString& newLabel)
            {
                if(newLabel != model.metadata.label())
                    m_commandDispatcher->submitCommand(new ChangeElementLabel<T>{path(model), newLabel});
            });

            connect(this, &MetadataWidget::commentsChanged,
                    [&](const QString& newComments)
            {
                if(newComments != model.metadata.comment())
                    m_commandDispatcher->submitCommand(new ChangeElementComments<T>{path(model), newComments});
            });

            connect(this, &MetadataWidget::colorChanged,
                    [&](ColorRef newColor)
            {
                if(newColor != model.metadata.color())
                    m_commandDispatcher->submitCommand(new ChangeElementColor<T>{path(model), newColor});
            });
        }

        void setScriptingName(QString arg);
        void updateAsked();

    signals:
        void scriptingNameChanged(QString arg);
        void labelChanged(QString arg);
        void commentsChanged(QString arg);
        void colorChanged(ColorRef arg);

    private:
        const ModelMetadata* m_metadata;
        CommandDispatcher<>* m_commandDispatcher;

        QLineEdit* m_scriptingNameLine {};
        QLineEdit* m_labelLine {};
        QPushButton* m_colorButton {};
        CommentEdit* m_comments {};
        QPixmap m_colorButtonPixmap {4 * m_colorIconSize / 3, 4 * m_colorIconSize / 3};
        static const int m_colorIconSize
        {
            21
        };
        bool m_cmtExpanded{false};
        QToolButton* m_cmtBtn{};

        color_widgets::ColorPaletteModel* m_palette{};

//        QString m_scriptingName;
};
}
