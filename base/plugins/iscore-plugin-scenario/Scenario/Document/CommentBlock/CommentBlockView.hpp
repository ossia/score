#pragma once

#include <QGraphicsTextItem>

class CommentBlockPresenter;

class CommentBlockView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        CommentBlockView(CommentBlockPresenter& presenter,
                         QGraphicsObject* parent);

        //~TimeNodeView() = default;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 3; }
        int type() const override
        { return static_type(); }

        const CommentBlockPresenter& presenter() const
        { return m_presenter;}

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        QRectF boundingRect() const override
        { return { -1., 1., 2., 2. }; }

    public slots:

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        CommentBlockPresenter& m_presenter;

        QGraphicsTextItem* m_textItem{};
};
