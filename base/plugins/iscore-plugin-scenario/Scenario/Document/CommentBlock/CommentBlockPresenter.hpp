#pragma once

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QPoint>
#include <Process/TimeValue.hpp>

class CommentBlockView;
class CommentBlockModel;
class QGraphicsObject;

class CommentBlockPresenter final :  public NamedObject
{
        Q_OBJECT
    public:
        CommentBlockPresenter(const CommentBlockModel& model,
                              QGraphicsObject* parentView,
                              QObject* parent);

        ~CommentBlockPresenter();

        const Id<CommentBlockModel>& id() const;
        int32_t id_val() const
        {
            return *id().val();
        }

        const CommentBlockModel& model() const
        {return m_model; }

        CommentBlockView* view() const
        {return m_view; }

        const TimeValue& date() const;

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    public slots:

    private:
        const CommentBlockModel& m_model;
        CommentBlockView* m_view{};
};
