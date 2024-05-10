#pragma once
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/audio/dummy_protocol.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
namespace Audio
{

class DummyFactory final : public AudioFactory
{
  SCORE_CONCRETE("13dabcc3-9cda-422f-a8c7-5fef5c220677")
public:
  ~DummyFactory() override { }
  bool available() const noexcept override { return true; }
  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
  }

  QString prettyName() const override { return QObject::tr("Dummy (No audio)"); };
  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    return std::make_shared<ossia::dummy_engine>(set.getRate(), set.getBufferSize());
  }

  static void updateLabel(QLabel& l, int bs, int rate)
  {
    l.setText(QString{"%1 ms (%2 hz)"}
                  .arg(1e3 * double(bs) / rate, 0, 'f', 3)
                  .arg(rate / double(bs), 0, 'f', 3));
  }

  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto timeLabel = new QLabel;
    updateLabel(*timeLabel, m.getBufferSize(), m.getRate());

    lay->addRow(QObject::tr("Tick duration"), timeLabel);
    auto buffer_sb = new QSpinBox{};
    {
      buffer_sb->setObjectName("BufferSize");
      lay->addRow(QObject::tr("Buffer size"), buffer_sb);
      buffer_sb->setRange(1, 10000);
      buffer_sb->setValue(m.getBufferSize());
    }

    auto rate_cb = addSampleRateWidget(*w, m, v);
    auto update_label = [timeLabel, rate_cb, buffer_sb] {
      updateLabel(*timeLabel, buffer_sb->value(), rate_cb->currentText().toInt());
    };
    QObject::connect(
        buffer_sb, &QSpinBox::valueChanged, w, [buffer_sb, &v, update_label] {
      v.BufferSizeChanged(buffer_sb->value());
      update_label();
    });
    QObject::connect(
        rate_cb, &QComboBox::currentIndexChanged, &v, [&v, rate_cb, update_label] {
      v.RateChanged(rate_cb->currentText().toInt());
      update_label();
    });

    con(m, &Audio::Settings::Model::BufferSizeChanged, timeLabel, update_label);
    con(m, &Audio::Settings::Model::RateChanged, timeLabel, update_label);

    return w;
  }
};
}
