#pragma once
#include <score/widgets/DoubleSlider.hpp>
#include <QCheckBox>
#include <QPushButton>
#include <array>
#include <score_plugin_engine_export.h>
namespace Control
{
struct SCORE_PLUGIN_ENGINE_EXPORT ToggleButton : public QPushButton
{
  public:
    ToggleButton(std::array<const char*, 2> alt, QWidget *parent)
      : QPushButton{parent}
    {
      alternatives[0] = alt[0];
      alternatives[1] = alt[1];
      setCheckable(true);

      connect(this, &QPushButton::toggled,
              this, [&] (bool b) {
        if(b)
        {
          setText(alternatives[1]);
        }
        else
        {
          setText(alternatives[0]);
        }
      });
      if(isChecked())
      {
        setText(alternatives[1]);
      }
      else
      {
        setText(alternatives[0]);
      }
    }
    std::array<QString, 2> alternatives;
  protected:
    void paintEvent(QPaintEvent* event) override;
};

struct SCORE_PLUGIN_ENGINE_EXPORT ValueSlider : public QSlider
{
  public:
    using QSlider::QSlider;
    bool moving = false;
  protected:
    void paintEvent(QPaintEvent* event) override;
};

struct SCORE_PLUGIN_ENGINE_EXPORT ValueDoubleSlider : public score::DoubleSlider
{
  public:
    using score::DoubleSlider::DoubleSlider;
    bool moving = false;
    double min{};
    double max{};
  protected:
    void paintEvent(QPaintEvent* event) override;
};

struct SCORE_PLUGIN_ENGINE_EXPORT ValueLogDoubleSlider : public score::DoubleSlider
{
  public:
    using score::DoubleSlider::DoubleSlider;
    bool moving = false;
    double min{};
    double max{};
  protected:
    void paintEvent(QPaintEvent* event) override;

};

struct SCORE_PLUGIN_ENGINE_EXPORT ComboSlider : public QSlider
{
    QStringList array;
  public:
    template<std::size_t N>
    ComboSlider(const std::array<const char*, N>& arr, QWidget* parent):
      QSlider{parent}
    {
        array.reserve(N);
        for(auto str : arr)
            array.push_back(str);
    }

    ComboSlider(const QStringList& arr, QWidget* parent);

    bool moving = false;
  protected:
    void paintEvent(QPaintEvent* event) override;
};


}
