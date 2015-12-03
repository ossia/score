#pragma once

#include <Process/TimeValue.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QObject>

class DataStream;
class JSONObject;
class QTextDocument;

class CommentBlockModel final : public IdentifiedObject<CommentBlockModel>
{
        Q_OBJECT

        ISCORE_METADATA("CommentBlockModel")

        ISCORE_SERIALIZE_FRIENDS(CommentBlockModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(CommentBlockModel, JSONObject)

        static QString description()
        { return QObject::tr("Comment Block"); }

    public:
        Selectable selection;

        CommentBlockModel(const Id<CommentBlockModel>& id,
                const TimeValue& date,
                double yPos,
                QObject* parent);

        template<typename DeserializerVisitor>
        CommentBlockModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        CommentBlockModel(const CommentBlockModel& source,
                          const Id<CommentBlockModel> &id,
                          QObject *parent);

        void setDate(const TimeValue& date);
        const TimeValue& date() const;

        double heightPercentage() const;
        void setHeightPercentage(double y);

        const QString content() const;
        void setContent(const QString content);

    signals:
        void dateChanged(const TimeValue&);
        void heightPercentageChanged(bool);
        void contentChanged(QString);

    public slots:

    private:
        TimeValue m_date{std::chrono::seconds{0}};
        double m_yposition{0};

        QString m_HTMLcontent{"Hello dear user !"};
};
