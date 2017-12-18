#pragma once
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneEvent>
#include <ossia/detail/math.hpp>
#include <QPainter>
#include <score_lib_base_export.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapButton
    : public QObject
    , public QGraphicsPixmapItem
{
    Q_OBJECT
    QPixmap m_pressed, m_released;
  public:
    QGraphicsPixmapButton(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  signals:
    void clicked();

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle
    : public QObject
    , public QGraphicsPixmapItem
{
    Q_OBJECT
    QPixmap m_pressed, m_released;
    bool m_toggled{};
  public:
    QGraphicsPixmapToggle(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  signals:
    void toggled(bool);

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsSlider
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT

    double m_value{};
    QRectF m_rect;
  public:
    double min{}, max{};
  private:
    bool m_grab;
  public:
    QGraphicsSlider(QGraphicsItem* parent);

    void setRect(QRectF r);
    void setValue(double v);
    double value() const;

    bool moving = false;
  signals:
    void valueChanged(double);
    void sliderMoved();
    void sliderReleased();

  private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsLogSlider
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    double m_value{};
    QRectF m_rect;
  public:
    double min{}, max{};
  private:
    bool m_grab;
  public:
    QGraphicsLogSlider(QGraphicsItem* parent);

    void setRect(QRectF r);
    void setValue(double v);
    double value() const;

    bool moving = false;
  signals:
    void valueChanged(double);
    void sliderMoved();
    void sliderReleased();

  private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;

};

class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    QRectF m_rect;
    int m_value{}, m_min{}, m_max{};
    bool m_grab;
  public:
    QGraphicsIntSlider(QGraphicsItem* parent);

    void setRect(QRectF r);
    void setValue(int v);
    void setRange(int min, int max);
    int value() const;

    bool moving = false;
  signals:
    void valueChanged(int);
    void sliderMoved();
    void sliderReleased();

  private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsComboSlider
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    QRectF m_rect;
  public:
    QStringList array;
  private:
    int m_value{};
    bool m_grab;
  public:
    template<std::size_t N>
    QGraphicsComboSlider(const std::array<const char*, N>& arr, QGraphicsItem* parent):
      QGraphicsItem{parent}
    {
      array.reserve(N);
      for(auto str : arr)
        array.push_back(str);

      this->setAcceptedMouseButtons(Qt::LeftButton);
    }

    QGraphicsComboSlider(QGraphicsItem* parent);

    void setRect(QRectF r);
    void setValue(int v);
    int value() const;

    bool moving = false;
  signals:
    void valueChanged(int);
    void sliderMoved();
    void sliderReleased();

  private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;

};

}
