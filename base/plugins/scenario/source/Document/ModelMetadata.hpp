#pragma once
#include <QObject>
#include <QColor>

/**
 * @brief The ModelMetadata class
 *
 * Metadata for the constraint, in order to make ConstraintModel lighter
 *
 */
class ModelMetadata : public QObject
{
        Q_OBJECT
        Q_PROPERTY(QString name
                   READ name
                   WRITE setName
                   NOTIFY nameChanged)

        Q_PROPERTY(QString comment
                   READ comment
                   WRITE setComment
                   NOTIFY commentChanged)

        Q_PROPERTY(QColor color
                   READ color
                   WRITE setColor
                   NOTIFY colorChanged)

        Q_PROPERTY(QString label
                   READ label
                   WRITE setLabel
                   NOTIFY labelChanged)

    public:
        ModelMetadata() = default;
        ModelMetadata(const ModelMetadata& other) :
            QObject {}
        {
            setName(other.name());
            setComment(other.comment());
            setColor(other.color());
        }

        ModelMetadata& operator= (const ModelMetadata& other)
        {
            setName(other.name());
            setComment(other.comment());
            setColor(other.color());

            return *this;
        }

        QString name() const;
        QString comment() const;
        QColor color() const;
        QString label() const;

    signals:
        void nameChanged(QString arg);
        void commentChanged(QString arg);
        void colorChanged(QColor arg);
        void labelChanged(QString arg);
        void metadataChanged();

    public slots:
        void setName(QString arg);
        void setComment(QString arg);
        void setColor(QColor arg);
        void setLabel(QString arg);


    private:
        QString m_scriptingName;
        QString m_comment;
        QColor m_color {Qt::black};
        QString m_label;
};

Q_DECLARE_METATYPE(ModelMetadata)
