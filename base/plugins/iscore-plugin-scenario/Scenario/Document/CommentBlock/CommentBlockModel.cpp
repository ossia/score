#include "CommentBlockModel.hpp"

#include <QTextDocument>

CommentBlockModel::CommentBlockModel(const Id<CommentBlockModel>& id,
                           const TimeValue& date,
                           double yPos,
                           QObject* parent):
    IdentifiedObject<CommentBlockModel>{id, "CommentBlockModel", parent},
    m_date{date},
    m_yposition{yPos}
{
}

CommentBlockModel::CommentBlockModel(const CommentBlockModel& source,
                                     const Id<CommentBlockModel>& id,
                                     QObject* parent):
    IdentifiedObject<CommentBlockModel>{id, "CommentBlockModel", parent}
{
    m_date = source.date();
    m_yposition = source.heightPercentage();
    m_HTMLcontent = source.content();

}

void CommentBlockModel::setDate(const TimeValue& date)
{
    if(date != m_date)
    {
        m_date = date;
        emit dateChanged(m_date);
    }
}

const TimeValue&CommentBlockModel::date() const
{
    return m_date;
}

double CommentBlockModel::heightPercentage() const
{
    return m_yposition;
}

void CommentBlockModel::setHeightPercentage(double y)
{
    if(y != m_yposition)
    {
        m_yposition = y;
        emit heightPercentageChanged(y);
    }
}

const QString CommentBlockModel::content() const
{
    return m_HTMLcontent;
}

void CommentBlockModel::setContent(const QString content)
{
    if(m_HTMLcontent == content)
        return;
    m_HTMLcontent = content;
    emit contentChanged(m_HTMLcontent);
}
