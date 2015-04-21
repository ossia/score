#pragma once
#include <QObject>
#include <QColor>
#include <QVariant>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

// TODO put in a scenario plugin interface
/**
 * @brief The ModelMetadata class
 *
 * Metadata for the constraint, in order to make ConstraintModel lighter
 *
 */
class ModelMetadata : public QObject
{

        friend void Visitor<Reader<DataStream>>::readFrom<ModelMetadata> (const ModelMetadata& ev);
        friend void Visitor<Reader<JSON>>::readFrom<ModelMetadata> (const ModelMetadata& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<ModelMetadata> (ModelMetadata& ev);
        friend void Visitor<Writer<JSON>>::writeTo<ModelMetadata> (ModelMetadata& ev);

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

        template<typename T>
        void addPluginMetadata(const T& data)
        {
            Q_ASSERT(std::none_of(std::begin(m_pluginsMetadata),
                                  std::end(m_pluginsMetadata),
                                  [&] (const QVariant& var) { return var.canConvert<T>(); }));
            m_pluginsMetadata.push_back(QVariant::fromValue(data));
            emit pluginMetaDataChanged();
        }

        template<typename T>
        void updatePluginMetadata(const T& data)
        {
            for(QVariant& elt : m_pluginsMetadata)
            {
                if(elt.canConvert<T>())
                {
                    elt = QVariant::fromValue(data);
                    emit pluginMetaDataChanged();
                }
            }

        }

        const QList<QVariant>& pluginMetadatas() const
        { return m_pluginsMetadata; }

    signals:
        void nameChanged(QString arg);
        void commentChanged(QString arg);
        void colorChanged(QColor arg);
        void labelChanged(QString arg);
        void metadataChanged();
        void pluginMetaDataChanged();

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

        QList<QVariant> m_pluginsMetadata;
};

Q_DECLARE_METATYPE(ModelMetadata)


#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
template<typename Element> void initPlugins(Element* e, QObject* obj)
{
    // We initialize the potential plug-ins of this document with this object's metadata if necessary.
    iscore::Document* doc = iscore::IDocument::documentFromObject(obj);

    for(auto& plugin : doc->model()->pluginModels())
    {
        if(plugin->canMakeMetadata(Element::staticMetaObject.className()))
            e->metadata.addPluginMetadata(plugin->makeMetadata(Element::staticMetaObject.className()));
    }
}
