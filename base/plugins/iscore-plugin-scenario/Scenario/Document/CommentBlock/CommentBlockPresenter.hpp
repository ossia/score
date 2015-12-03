#pragma once

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QPoint>
#include <Process/TimeValue.hpp>

class CommentBlockView;
class CommentBlockModel;
class QGraphicsObject;
class QTextDocument;

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

        bool isPressed() {return m_pressed;}
        const QPointF& pressedPoint() {return isPressed() ? m_clickedPoint : m_origin;}
        void setPressed(bool b) {m_pressed = b;}

    signals:
        void moved(const QPointF&);
        void released(const QPointF&);
        void doubleClicked();
        void editFinished(QString);

    public slots:
        void pressed(const QPointF&);
        void on_zoomRatioChanged(ZoomRatio newRatio);

    private:
        const CommentBlockModel& m_model;
        CommentBlockView* m_view{};

        bool m_pressed{false};
        QPointF m_origin{0,0};
        QPointF m_clickedPoint{m_origin};
};
