#pragma once

#include <QWidget>
#include <core/tools/ObjectPath.hpp>

class QVBoxLayout;
class QLineEdit;
class QLabel;
class QTextEdit;
class QPushButton;
class ModelMetadata;

class MetadataWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit MetadataWidget(ModelMetadata* metadata = 0, QWidget* parent = 0);

    QString scriptingName() const;

public slots:
    void setScriptingName(QString arg);

    void setType(QString type);

    void updateAsked();

signals:
    void scriptingNameChanged(QString arg);
    void labelChanged(QString arg);
    void commentsChanged(QString arg);

private:

        ModelMetadata* m_metadata;

        QLabel* m_typeLb{};
        QLineEdit* m_scriptingNameLine{};
        QLineEdit* m_labelLine{};
        QPushButton* m_colorButton{};
        QTextEdit* m_comments{};
        QPixmap m_colorButtonPixmap{4 * m_colorIconSize / 3, 4 * m_colorIconSize / 3};
        static const int m_colorIconSize{21};

//        QString m_scriptingName;
};
