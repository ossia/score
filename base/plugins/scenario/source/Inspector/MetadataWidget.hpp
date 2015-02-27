#pragma once

#include <QWidget>
#include <core/tools/ObjectPath.hpp>
#include <core/interface/document/DocumentInterface.hpp>
#include <core/presenter/command/SerializableCommand.hpp>

#include "Commands/Metadata/ChangeElementLabel.hpp"
#include "Commands/Metadata/ChangeElementName.hpp"
#include "Commands/Metadata/ChangeElementComments.hpp"
#include "Commands/Metadata/ChangeElementColor.hpp"

class QVBoxLayout;
class QLineEdit;
class QLabel;
class CommentEdit;
class QPushButton;
class ModelMetadata;

class MetadataWidget : public QWidget
{
        Q_OBJECT

    public:
        explicit MetadataWidget(ModelMetadata* metadata = 0, QWidget* parent = 0);

        QString scriptingName() const;

        template<typename T>
        void setupConnections(T* model)
        {
            using namespace Scenario::Command;
            using namespace iscore::IDocument;
            connect(this, &MetadataWidget::scriptingNameChanged,
                    [&](QString newName)
            {
                if(newName != model->metadata.name())
                    submitCommand(new ChangeElementName<T>{path(model), newName});
            });

            connect(this, &MetadataWidget::labelChanged,
                    [&](QString newLabel)
            {
                if(newLabel != model->metadata.label())
                    submitCommand(new ChangeElementLabel<T>{path(model), newLabel});
            });

            connect(this, &MetadataWidget::commentsChanged,
                    [&](QString newComments)
            {
                if(newComments != model->metadata.comment())
                    submitCommand(new ChangeElementComments<T>{path(model), newComments});
            });

            connect(this, &MetadataWidget::colorChanged,
                    [&](QColor newColor)
            {
                if(newColor != model->metadata.color())
                    submitCommand(new ChangeElementColor<T>{path(model), newColor});
            });
        }

    public slots:
        void setScriptingName(QString arg);

        void changeColor();

        void setType(QString type);

        void updateAsked();

    signals:
        void scriptingNameChanged(QString arg);
        void labelChanged(QString arg);
        void commentsChanged(QString arg);
        void colorChanged(QColor arg);

        void submitCommand(iscore::SerializableCommand*);

    private:

        ModelMetadata* m_metadata;

        QLabel* m_typeLb {};
        QLineEdit* m_scriptingNameLine {};
        QLineEdit* m_labelLine {};
        QPushButton* m_colorButton {};
        CommentEdit* m_comments {};
        QPixmap m_colorButtonPixmap {4 * m_colorIconSize / 3, 4 * m_colorIconSize / 3};
        static const int m_colorIconSize
        {
            21
        };

//        QString m_scriptingName;
};
