#pragma once



#include <QColor>
#include <qnamespace.h>
#include <QObject>
#include <QString>

class DataStream;
class JSONObject;

/**
 * @brief The ModelMetadata class
 */
class ModelMetadata : public QObject
{
        ISCORE_SERIALIZE_FRIENDS(ModelMetadata, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ModelMetadata, JSONObject)

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
            setLabel(other.label());
        }

        ModelMetadata& operator= (const ModelMetadata& other)
        {
            setName(other.name());
            setComment(other.comment());
            setColor(other.color());
            setLabel(other.label());

            return *this;
        }

        const QString& name() const;
        const QString& comment() const;
        const QColor& color() const;
        const QString& label() const;

    signals:
        void nameChanged(const QString& arg);
        void commentChanged(const QString& arg);
        void colorChanged(const QColor& arg);
        void labelChanged(const QString& arg);
        void metadataChanged();

    public slots:
        void setName(const QString& arg);
        void setComment(const QString& arg);
        void setColor(const QColor& arg);
        void setLabel(const QString& arg);


    private:
        QString m_scriptingName;
        QString m_comment;
        QColor m_color {Qt::gray};
        QString m_label;
};

Q_DECLARE_METATYPE(ModelMetadata)

