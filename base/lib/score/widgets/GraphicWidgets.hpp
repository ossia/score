#pragma once
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneEvent>
#include <ossia/detail/math.hpp>
#include <QPainter>
#include <score_lib_base_export.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapButton final
    : public QObject
    , public QGraphicsPixmapItem
{
    Q_OBJECT
    const QPixmap m_pressed, m_released;
  public:
    QGraphicsPixmapButton(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  Q_SIGNALS:
    void clicked();

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle final
    : public QObject
    , public QGraphicsPixmapItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

    const QPixmap m_pressed, m_released;
    bool m_toggled{};
  public:
    QGraphicsPixmapToggle(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

    void toggle();

  Q_SIGNALS:
    void toggled(bool);

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsSlider final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

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
  Q_SIGNALS:
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

class SCORE_LIB_BASE_EXPORT QGraphicsLogSlider final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

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
  Q_SIGNALS:
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

class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
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
  Q_SIGNALS:
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

class SCORE_LIB_BASE_EXPORT QGraphicsComboSlider final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
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
  Q_SIGNALS:
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
