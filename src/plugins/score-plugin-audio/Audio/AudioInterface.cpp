#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

namespace Audio
{

AudioFactory::~AudioFactory() { }

void AudioFactory::addBufferSizeWidget(QWidget& widg, Settings::Model& m, Settings::View& v)
{
  auto cb = new QComboBox{&widg};
  cb->setObjectName("BufferSize");
  for (auto val : {32, 64, 128, 256, 512, 1024, 2048})
  {
    cb->addItem(QString::number(val));
  }
  ((QFormLayout*)widg.layout())->addRow(QObject::tr("Buffer size"), cb);
  cb->setCurrentIndex(cb->findText(QString::number(m.getBufferSize())));
  QObject::connect(cb, SignalUtils::QComboBox_currentIndexChanged_int(), &widg, [cb, &v](int i) {
    v.BufferSizeChanged(cb->itemText(i).toInt());
  });
}

void AudioFactory::addSampleRateWidget(QWidget& widg, Settings::Model& m, Settings::View& v)
{
  auto cb = new QComboBox{&widg};
  cb->setObjectName("Rate");
  for (auto val : {44100, 48000, 88200, 96000, 192000})
  {
    cb->addItem(QString::number(val));
  }
  ((QFormLayout*)widg.layout())->addRow(QObject::tr("Rate"), cb);
  cb->setCurrentIndex(cb->findText(QString::number(m.getRate())));
  QObject::connect(cb, SignalUtils::QComboBox_currentIndexChanged_int(), &widg, [cb, &v](int i) {
    v.RateChanged(cb->itemText(i).toInt());
  });
}
}
