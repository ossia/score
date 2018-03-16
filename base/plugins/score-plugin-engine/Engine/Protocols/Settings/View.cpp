// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Engine/Protocols/Settings/Model.hpp>
#include <Engine/Protocols/Settings/View.hpp>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMenu>
#include <QFormLayout>
#include <score/widgets/SignalUtils.hpp>
#if __has_include(<portaudio.h>)
#include <portaudio.h>
#endif
namespace Audio::Settings
{
View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;

  SETTINGS_UI_NUM_COMBOBOX_SETUP("Rate", Rate, (std::vector<int>{44100, 48000, 88200, 96000}));
  SETTINGS_UI_NUM_COMBOBOX_SETUP("BufferSize", BufferSize, (std::vector<int>{64, 128, 256, 512}));

  QStringList raw_cards_in, cards_in;
  std::vector<int> indices_in;

  QStringList raw_cards_out, cards_out;
  std::vector<int> indices_out;
  Pa_Initialize();

  for(int i = 0; i < Pa_GetHostApiCount(); i++)
  {
    auto dev = Pa_GetHostApiInfo(i);
    QString dev_text;
    switch(dev->type)
    {
      case PaHostApiTypeId::paInDevelopment: continue;
      case PaHostApiTypeId::paDirectSound: dev_text = "DirectSound"; break;
      case PaHostApiTypeId::paMME: dev_text = "MME"; break;
      case PaHostApiTypeId::paASIO: dev_text = "ASIO"; break;
      case PaHostApiTypeId::paSoundManager: dev_text = "SoundManager"; break;
      case PaHostApiTypeId::paCoreAudio: dev_text = "CoreAudio"; break;
      case PaHostApiTypeId::paOSS: dev_text = "OSS"; break;
      case PaHostApiTypeId::paALSA: dev_text = "ALSA"; break;
      case PaHostApiTypeId::paAL: dev_text = "OpenAL"; break;
      case PaHostApiTypeId::paBeOS: dev_text = "BeOS"; break;
      case PaHostApiTypeId::paWDMKS: dev_text = "WDMKS"; break;
      case PaHostApiTypeId::paJACK: dev_text = "Jack"; break;
      case PaHostApiTypeId::paWASAPI: dev_text = "WASAPI"; break;
      case PaHostApiTypeId::paAudioScienceHPI: dev_text = "HPI"; break;
    }

    for(int card = 0; card < dev->deviceCount; card++)
    {
      auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
      auto dev = Pa_GetDeviceInfo(dev_idx);
      auto raw_name = QString::fromLocal8Bit(Pa_GetDeviceInfo(dev_idx)->name);
      if(dev->maxInputChannels > 0)
      {
        cards_in.push_back(
              "(" + dev_text + ") " + raw_name);
        raw_cards_in.push_back(raw_name);
        indices_in.push_back(dev_idx);
      }
      if(dev->maxOutputChannels > 0)
      {
        cards_out.push_back(
              "(" + dev_text + ") " + raw_name);
        raw_cards_out.push_back(raw_name);
        indices_out.push_back(dev_idx);
      }
    }

  }

  Pa_Terminate();
  qDebug() << cards_in << cards_out;

  m_CardIn = new QComboBox{m_widg};
  for(int i = 0; i < cards_in.size(); i++)
    m_CardIn->addItem(cards_in[i], indices_in[i]);
  lay->addRow(tr("Device"), m_CardIn);
  connect(m_CardIn, SignalUtils::QComboBox_currentIndexChanged_int(), this,
          [=] (int i) {
    CardInChanged( raw_cards_in[i] );
  } );
  m_CardOut = new QComboBox{m_widg};
  for(int i = 0; i < cards_out.size(); i++)
    m_CardOut->addItem(cards_out[i], indices_out[i]);
  lay->addRow(tr("Device"), m_CardOut);
  connect(m_CardOut, SignalUtils::QComboBox_currentIndexChanged_int(), this,
          [=] (int i) {
    CardOutChanged( raw_cards_out[i] );
  } );

  m_widg->setLayout(lay);

}

QWidget* View::getWidget()
{
  return m_widg;
}
SETTINGS_UI_NUM_COMBOBOX_IMPL(Rate)
SETTINGS_UI_NUM_COMBOBOX_IMPL(BufferSize)
void View::setCardIn(QString val) {
  int idx = m_CardIn->findData(QVariant::fromValue(val));
  if(idx != -1 && idx != m_CardIn->currentIndex())
    m_CardIn->setCurrentIndex(idx);
  else { idx = m_CardIn->findText(val);
    if (idx != -1 && idx != m_CardIn->currentIndex())
      m_CardIn->setCurrentIndex(idx);
  }
}
void View::setCardOut(QString val) {
  int idx = m_CardOut->findData(QVariant::fromValue(val));
  if(idx != -1 && idx != m_CardOut->currentIndex())
    m_CardOut->setCurrentIndex(idx);
  else { idx = m_CardOut->findText(val);
    if (idx != -1 && idx != m_CardOut->currentIndex())
      m_CardOut->setCurrentIndex(idx);
  }
}
}
