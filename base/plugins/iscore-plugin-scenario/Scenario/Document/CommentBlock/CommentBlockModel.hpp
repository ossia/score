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

        QString m_HTMLcontent{"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\np, li { white-space: pre-wrap; }\n</style></head><body style=\" font-family:'Ubuntu'; font-size:10pt; font-weight:400; font-style:normal;\">\n<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">New Comment</p></body></html>"};
};
