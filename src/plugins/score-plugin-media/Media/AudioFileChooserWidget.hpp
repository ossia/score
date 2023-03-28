#pragma once

#include <Process/Dataflow/ControlWidgets.hpp>

#include <Media/MediaFileHandle.hpp>

#include <score_plugin_media_export.h>
namespace Media::Sound
{
struct WaveformComputer;
}
namespace score
{

class SCORE_PLUGIN_MEDIA_EXPORT QGraphicsWaveformButton final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsWaveformButton)

  bool m_pressed{};

public:
  QGraphicsWaveformButton(QGraphicsItem* parent);
  ~QGraphicsWaveformButton();

  void pressed() E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, pressed);
  void dropped(QString filename) E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, dropped, filename);
  void bang();

  const QString& text() const noexcept { return m_string; }
  void setFile(const QString& s);
  QRectF boundingRect() const override;

private:
  void on_finishedDecoding();
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      final override;
  QRectF m_rect;
  QString m_string;
  std::shared_ptr<Media::AudioFile> m_file;
  QVector<QImage*> m_images;
  Media::Sound::WaveformComputer* m_computer{};
};

}
namespace Media
{

struct AudioFileChooser : WidgetFactory::FileChooser
{
  template <typename T, typename Control_T>
  static score::QGraphicsWaveformButton* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto bt = new score::QGraphicsWaveformButton{parent};
    auto on_open = [bt, &inlet, &ctx] {
      auto filename
          = QFileDialog::getOpenFileName(nullptr, "Open File", {}, inlet.filters());
      if(filename.isEmpty())
        return;

      CommandDispatcher<>{ctx.commandStack}
          .submit<WidgetFactory::SetControlValue<Control_T>>(
              inlet, filename.toStdString());
    };
    auto on_set = [bt, &inlet, &ctx](const QString& filename) {
      if(filename.isEmpty())
        return;

      CommandDispatcher<>{ctx.commandStack}
          .submit<WidgetFactory::SetControlValue<Control_T>>(
              inlet, filename.toStdString());
    };
    QObject::connect(bt, &score::QGraphicsWaveformButton::pressed, &inlet, on_open);
    QObject::connect(bt, &score::QGraphicsWaveformButton::dropped, &inlet, on_set);
    auto set = [=](const ossia::value& val) {
      auto str = QString::fromStdString(ossia::convert<std::string>(val));
      if(str != bt->text())
      {
        if(!str.isEmpty())
          bt->setFile(str);
        else
          bt->setFile({});
      }
    };

    set(slider.value());
    QObject::connect(&inlet, &Control_T::valueChanged, bt, set);

    return bt;
  }
};
}
