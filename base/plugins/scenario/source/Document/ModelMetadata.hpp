#pragma once
#include <QObject>
#include <QColor>
#include <QVariant>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

/**
 * @brief The ModelMetadata class
 */
class ModelMetadata : public QObject
{
        friend void Visitor<Reader<DataStream>>::readFrom<ModelMetadata> (const ModelMetadata& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<ModelMetadata> (const ModelMetadata& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<ModelMetadata> (ModelMetadata& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<ModelMetadata> (ModelMetadata& ev);

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

