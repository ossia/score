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
        { return QGraphicsItem::UserType + 42; } // TODO : change 42 by the appropriate number
        int type() const override
        { return static_type(); }

        const CommentBlockPresenter& presenter() const
        { return m_presenter;}

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        QRectF boundingRect() const override;

    public slots:
        void setSelected(bool b);
        void setHtmlContent(QString htmlText);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt) override;


    private:
        void SetTextInteraction(bool on, bool selectAll = false);

        CommentBlockPresenter& m_presenter;

        QGraphicsTextItem* m_textItem{};
        bool m_selected{false};
};
