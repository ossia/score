#pragma once
#include <ossia/audio/dummy_protocol.hpp>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>
#include <score/tools/Bind.hpp>
#include <QFormLayout>
#include <QSlider>
#include <QLabel>
namespace Audio
{

class DummyFactory final : public AudioFactory
{
  SCORE_CONCRETE("13dabcc3-9cda-422f-a8c7-5fef5c220677")
public:
  ~DummyFactory() override { }
  bool available() const noexcept override { return true; }

  QString prettyName() const override { return QObject::tr("Dummy (No audio)"); };
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    return std::make_unique<ossia::dummy_engine>(set.getRate(), set.getBufferSize());
  }

  static void updateLabel(QLabel& l, int bs, int rate) {
    l.setText(QString{"%1 ms"}.arg(1e3 * double(bs) / rate, 0, 'f', 3));
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto timeLabel = new QLabel;
    con(m, &Audio::Settings::Model::BufferSizeChanged,
        timeLabel, [=, &m] (int b) {
      updateLabel(*timeLabel, b, m.getRate());
    });
    con(m, &Audio::Settings::Model::RateChanged,
        timeLabel, [=, &m] (int r) {
      updateLabel(*timeLabel, m.getBufferSize(), r);
    });

    updateLabel(*timeLabel, m.getBufferSize(), m.getRate());

    lay->addRow(QObject::tr("Tick duration"), timeLabel);
    {
      auto cb = new QSlider{Qt::Horizontal};
      cb->setObjectName("BufferSize");
      lay->addRow(QObject::tr("Buffer size"), cb);
      cb->setRange(16, 10000);
      cb->setValue(m.getBufferSize());
      QObject::connect(cb, &QSlider::valueChanged,
                       w, [cb, &v, &m, timeLabel] (int i) {
        v.BufferSizeChanged(cb->value());
        updateLabel(*timeLabel, i, m.getRate());
      });
    }

    addSampleRateWidget(*w, m, v);

    return w;
  }
};
}
