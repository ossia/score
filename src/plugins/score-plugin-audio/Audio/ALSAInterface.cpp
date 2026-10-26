#include <Audio/ALSAInterface.hpp>
#if defined(OSSIA_AUDIO_ALSA)
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/libasound.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <cerrno>

namespace Audio
{
namespace
{
constexpr int standard_rates[]{22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
constexpr int standard_sizes[]{16, 32, 48, 64, 96, 128, 256, 512, 1024, 2048, 4096};

// The same choices as Alsa_pcmi::set_hwpar, so that what is listed opens.
bool restrict_like_the_engine(
    const ossia::libasound& snd, snd_pcm_t* pcm, snd_pcm_hw_params_t* hw)
{
  if(snd.pcm_hw_params_any(pcm, hw) < 0)
    return false;
  snd.pcm_hw_params_set_periods_integer(pcm, hw);

  bool access = false;
  for(auto a : {SND_PCM_ACCESS_MMAP_NONINTERLEAVED, SND_PCM_ACCESS_MMAP_INTERLEAVED,
                SND_PCM_ACCESS_MMAP_COMPLEX})
    if(snd.pcm_hw_params_set_access(pcm, hw, a) == 0)
    {
      access = true;
      break;
    }
  if(!access)
    return false;

  for(auto f : {SND_PCM_FORMAT_FLOAT_LE, SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S32_BE,
                SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S24_3BE, SND_PCM_FORMAT_S16_LE,
                SND_PCM_FORMAT_S16_BE})
    if(snd.pcm_hw_params_set_format(pcm, hw, f) == 0)
      return true;
  return false;
}

AlsaStream probe(const char* name, snd_pcm_stream_t stream)
{
  auto& snd = ossia::libasound::instance();
  AlsaStream res;

  snd_pcm_t* pcm{};
  if(int err = snd.pcm_open(&pcm, name, stream, SND_PCM_NONBLOCK); err < 0)
  {
    res.present = res.busy = (err == -EBUSY);
    return res;
  }

  snd_pcm_hw_params_t* hw{};
  snd_pcm_hw_params_t* at_rate{};
  snd_alloca(&hw, snd, pcm_hw_params);
  snd_alloca(&at_rate, snd, pcm_hw_params);
  if(restrict_like_the_engine(snd, pcm, hw))
  {
    unsigned int cmin{}, cmax{};
    snd.pcm_hw_params_get_channels_min(hw, &cmin);
    snd.pcm_hw_params_get_channels_max(hw, &cmax);
    // Plugins such as plughw report "any count".
    res.channels = {int(cmin), int(std::min(cmax, 64u))};

    for(int rate : standard_rates)
    {
      if(snd.pcm_hw_params_test_rate(pcm, hw, rate, 0) != 0)
        continue;
      snd.pcm_hw_params_copy(at_rate, hw);
      if(snd.pcm_hw_params_set_rate(pcm, at_rate, rate, 0) != 0)
        continue;

      std::vector<int> sizes;
      for(int bs : standard_sizes)
      {
        if(snd.pcm_hw_params_test_period_size(pcm, at_rate, bs, 0) != 0)
          continue;
        // The engine asks for 2 periods and falls back to 3 or 4.
        for(int periods : {2, 3, 4})
          if(snd.pcm_hw_params_test_buffer_size(pcm, at_rate, bs * periods) == 0)
          {
            sizes.push_back(bs);
            break;
          }
      }
      if(!sizes.empty())
      {
        res.rates.push_back(rate);
        res.buffer_sizes[rate] = std::move(sizes);
      }
    }
    res.present = !res.rates.empty();
  }
  snd.pcm_close(pcm);
  return res;
}

// Rates and sizes both directions accept; one alone when the other is unused.
std::vector<int> common_rates(const AlsaStream* a, const AlsaStream* b)
{
  if(!a)
    std::swap(a, b);
  if(!a)
    return {};
  std::vector<int> res;
  for(int r : a->rates)
    if(!b || ossia::contains(b->rates, r))
      res.push_back(r);
  return res;
}

std::vector<int> common_sizes(const AlsaStream* a, const AlsaStream* b, int rate)
{
  if(!a)
    std::swap(a, b);
  if(!a)
    return {};
  auto it = a->buffer_sizes.find(rate);
  if(it == a->buffer_sizes.end())
    return {};
  std::vector<int> res;
  for(int s : it->second)
  {
    if(b)
    {
      auto bit = b->buffer_sizes.find(rate);
      if(bit == b->buffer_sizes.end() || !ossia::contains(bit->second, s))
        continue;
    }
    res.push_back(s);
  }
  return res;
}

// Names saved by another backend (miniaudio's ":3,0", PortAudio's device
// names) mean nothing here.
bool is_alsa_name(const QString& name)
{
  return name.contains("CARD=") || name == "default";
}

QString card_of(const QString& raw_name)
{
  // hw:CARD=III,DEV=0 -> III
  auto start = raw_name.indexOf("CARD=");
  if(start < 0)
    return raw_name;
  start += 5;
  auto end = raw_name.indexOf(',', start);
  return raw_name.mid(start, end < 0 ? -1 : end - start);
}
}

class ALSAWidget : public QWidget
{
  ALSAFactory& m_factory;
  Audio::Settings::Model& m_model;
  Audio::Settings::View& m_view;
  score::SettingsCommandDispatcher& m_disp;

  QComboBox* m_out{};
  QComboBox* m_in{};
  QComboBox* m_rate{};
  QComboBox* m_bs{};
  QSpinBox* m_outCount{};
  QSpinBox* m_inCount{};
  QLabel* m_info{};

  // The pending selection: commands are only committed when the dialog is
  // accepted, the model still holds the previous values until then.
  QString m_cardOut;
  QString m_cardIn;

public:
  ALSAWidget(
      ALSAFactory& fact, Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& disp, QWidget* parent = nullptr)
      : QWidget{parent}
      , m_factory{fact}
      , m_model{m}
      , m_view{v}
      , m_disp{disp}
      , m_cardOut{m.getCardOut()}
      , m_cardIn{m.getCardIn()}
  {
    auto lay = new QFormLayout{this};

    m_out = new QComboBox{this};
    m_in = new QComboBox{this};
    lay->addRow(tr("Output device"), m_out);
    lay->addRow(tr("Input device"), m_in);

    m_outCount = new QSpinBox{this};
    m_inCount = new QSpinBox{this};
    lay->addRow(tr("Output channels"), m_outCount);
    lay->addRow(tr("Input channels"), m_inCount);

    m_rate = fact.addSampleRateWidget(*this, m, v);
    m_bs = fact.addBufferSizeWidget(*this, m, v);

    auto rescan = new QPushButton{tr("Rescan"), this};
    lay->addRow(rescan);
    m_info = new QLabel{this};
    m_info->setWordWrap(true);
    lay->addRow(m_info);

    m_outCount->setValue(m.getDefaultOut());
    m_inCount->setValue(m.getDefaultIn());

    if(!is_alsa_name(m_cardOut) && !is_alsa_name(m_cardIn))
    {
      // Coming from another backend.
      if(auto card = fact.defaultCard())
      {
        m_cardOut = card->raw_name;
        m_cardIn.clear();
        m_outCount->setValue(2);
        m_inCount->setValue(0);
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, m_cardOut);
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, m_cardIn);
      }
    }

    if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_ALSA"))
      fillDevices();

    connect(rescan, &QPushButton::clicked, this, [this] {
      m_factory.rescan();
      fillDevices();
    });
    connect(
        m_out, SignalUtils::QComboBox_currentIndexChanged_int(), this, [this](int) {
          m_cardOut = m_out->currentData().toString();
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m_model, m_cardOut);
          updateControls();
        });
    connect(
        m_in, SignalUtils::QComboBox_currentIndexChanged_int(), this, [this](int) {
          m_cardIn = m_in->currentData().toString();
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m_model, m_cardIn);
          updateControls();
        });
    connect(
        m_outCount, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int i) {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m_model, i);
        });
    connect(
        m_inCount, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int i) {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m_model, i);
        });
    // The buffer sizes depend on the rate.
    connect(
        m_rate, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int) { updateBufferSizes(); });
  }

private:
  static void addDevice(QComboBox* combo, const AlsaCard& card, const AlsaStream& s)
  {
    QString label = card.pretty_name;
    if(card.converted)
      label += tr(" (with conversion)");
    if(s.busy)
      label += tr(" (in use)");
    combo->addItem(label, card.raw_name);
  }

  static void select(QComboBox* combo, const QString& name)
  {
    int idx = combo->findData(name);
    if(idx < 0 && !name.isEmpty())
    {
      // A device that is not plugged in keeps its place in the settings.
      combo->addItem(tr("%1 (not found)").arg(name), name);
      idx = combo->count() - 1;
    }
    combo->setCurrentIndex(std::max(idx, 0));
  }

  void fillDevices()
  {
    QSignalBlocker b1{m_out}, b2{m_in};
    m_out->clear();
    m_in->clear();
    m_out->addItem(tr("No device"), QString{});
    m_in->addItem(tr("No device"), QString{});
    for(bool converted : {false, true})
    {
      if(converted)
      {
        m_out->insertSeparator(m_out->count());
        m_in->insertSeparator(m_in->count());
      }
      for(const auto& card : m_factory.devices)
      {
        if(card.converted != converted)
          continue;
        if(card.playback.present)
          addDevice(m_out, card, card.playback);
        if(card.capture.present)
          addDevice(m_in, card, card.capture);
      }
    }
    select(m_out, m_cardOut);
    select(m_in, m_cardIn);
    updateControls();
  }

  const AlsaStream* stream(const QString& name, bool capture) const
  {
    if(auto card = m_factory.find(name))
    {
      auto& s = capture ? card->capture : card->playback;
      if(s.present && !s.rates.empty())
        return &s;
    }
    return nullptr;
  }

  static void fill(QComboBox* combo, const std::vector<int>& values, int keep)
  {
    QSignalBlocker block{combo};
    combo->clear();
    for(int v : values)
      combo->addItem(QString::number(v), v);
    int idx = combo->findData(keep);
    if(idx < 0)
    {
      // The nearest value the device accepts.
      int best = -1;
      for(int i = 0; i < combo->count(); i++)
        if(best < 0
           || std::abs(combo->itemData(i).toInt() - keep)
                  < std::abs(combo->itemData(best).toInt() - keep))
          best = i;
      idx = best;
    }
    combo->setCurrentIndex(idx);
  }

  void updateControls()
  {
    const auto out = stream(m_cardOut, false);
    const auto in = stream(m_cardIn, true);

    // Unknown capabilities (busy since the start, or not plugged in): keep
    // the current settings rather than offering defaults.
    auto rates = common_rates(out, in);
    if(rates.empty())
      rates = {m_rate->currentText().toInt() > 0 ? m_rate->currentText().toInt()
                                                 : m_model.getRate()};
    fill(m_rate, rates, m_rate->currentText().toInt() > 0 ? m_rate->currentText().toInt()
                                                          : m_model.getRate());
    m_view.RateChanged(m_rate->currentData().toInt());
    updateBufferSizes();

    auto channels = [](QSpinBox* sb, const AlsaStream* s, bool used) {
      QSignalBlocker block{sb};
      const int prev = sb->value();
      if(!used)
        sb->setRange(0, 0);
      else if(s)
        sb->setRange(0, s->channels.max);
      else
        sb->setRange(0, 64);
      sb->setValue(std::clamp(prev, sb->minimum(), sb->maximum()));
    };
    channels(m_outCount, out, !m_cardOut.isEmpty());
    channels(m_inCount, in, !m_cardIn.isEmpty());
    if(m_outCount->value() == 0 && out)
      m_outCount->setValue(std::min(2, out->channels.max));
    m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
        m_model, m_outCount->value());
    m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
        m_model, m_inCount->value());

    updateInfo(out, in);
  }

  void updateBufferSizes()
  {
    const auto out = stream(m_cardOut, false);
    const auto in = stream(m_cardIn, true);
    const int rate = m_rate->currentData().toInt();
    const int keep = m_bs->currentText().toInt() > 0 ? m_bs->currentText().toInt()
                                                     : m_model.getBufferSize();
    auto sizes = common_sizes(out, in, rate);
    if(sizes.empty())
      sizes = {keep};
    fill(m_bs, sizes, keep);
    m_view.BufferSizeChanged(m_bs->currentData().toInt());
    m_view.RateChanged(rate);
  }

  void updateInfo(const AlsaStream* out, const AlsaStream* in)
  {
    QStringList lines;
    auto card = [this](const QString& n) { return m_factory.find(n); };
    for(auto [name, dir] : {std::pair{m_cardOut, false}, std::pair{m_cardIn, true}})
    {
      if(auto c = card(name))
        if((dir ? c->capture : c->playback).busy)
          lines << tr("%1 is in use, by score or another program: the choices shown "
                      "are the ones it offered when it was last free.")
                       .arg(c->pretty_name);
    }
    if(!m_cardOut.isEmpty() && !m_cardIn.isEmpty()
       && card_of(m_cardOut) != card_of(m_cardIn))
      lines << tr("Input and output are on different cards. Their clocks differ: "
                  "small buffer sizes may not run, and long sessions will click "
                  "every few minutes.");
    if(m_cardOut.isEmpty() && m_cardIn.isEmpty())
      lines << tr("Select an output or an input device.");
    m_info->setText(lines.join("\n"));
  }
};

ALSAFactory::ALSAFactory() { }

ALSAFactory::~ALSAFactory() { }

const AlsaCard* ALSAFactory::defaultCard() const noexcept
{
  for(auto& card : devices)
    if(!card.converted && card.playback.present && !card.playback.busy)
      return &card;
  return nullptr;
}

const AlsaCard* ALSAFactory::find(const QString& raw_name) const noexcept
{
  if(raw_name.isEmpty())
    return nullptr;
  for(auto& d : devices)
    if(d.raw_name == raw_name)
      return &d;
  return nullptr;
}

void ALSAFactory::initialize(
    Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  if(!m_scanned)
    rescan();

  // A card that is not plugged in keeps its place: the engine reports it
  // missing instead of another card silently replacing it.
  const auto& out = set.getCardOut();
  const auto& in = set.getCardIn();
  if((out.isEmpty() || is_alsa_name(out)) && (in.isEmpty() || is_alsa_name(in))
     && !(out.isEmpty() && in.isEmpty()))
    return;

  auto card = defaultCard();
  if(!card)
    return;

  qDebug() << "ALSA: using" << card->raw_name;
  set.setCardOut(card->raw_name);
  set.setCardIn(QString{});
  set.setDefaultIn(0);
  // The count score uses: the engine opens every channel a device requires.
  set.setDefaultOut(std::min(2, card->playback.channels.max));

  const auto& rates = card->playback.rates;
  const int rate = ossia::contains(rates, 48000) ? 48000 : rates.front();
  set.setRate(rate);
  const auto& sizes = card->playback.buffer_sizes.at(rate);
  int bs = set.getBufferSize();
  if(!ossia::contains(sizes, bs))
    bs = ossia::contains(sizes, 512) ? 512 : sizes.back();
  set.setBufferSize(bs);
  set.changed();
}

bool ALSAFactory::available() const noexcept
{
  try
  {
    ossia::libasound::instance();
    return true;
  }
  catch(...)
  {
    return false;
  }
}

void ALSAFactory::rescan()
{
  m_scanned = true;
  devices.clear();

  auto& snd = ossia::libasound::instance();

  auto add = [&](const QString& raw_name, const QString& pretty_name, bool converted) {
    AlsaCard card;
    card.raw_name = raw_name;
    card.pretty_name = pretty_name;
    card.converted = converted;
    const auto name = raw_name.toStdString();
    card.playback = probe(name.c_str(), SND_PCM_STREAM_PLAYBACK);
    card.capture = probe(name.c_str(), SND_PCM_STREAM_CAPTURE);

    // A busy direction keeps what it offered when it was last free.
    if(auto it = m_lastFree.find(card.raw_name); it != m_lastFree.end())
    {
      for(auto stream : {&AlsaCard::playback, &AlsaCard::capture})
      {
        if((card.*stream).busy && (it->second.*stream).present)
        {
          card.*stream = it->second.*stream;
          (card.*stream).busy = true;
        }
      }
    }
    if(!card.playback.busy && !card.capture.busy)
      m_lastFree[card.raw_name] = card;

    if(card.playback.present || card.capture.present)
      devices.push_back(std::move(card));
  };

  int idx = -1;
  while(snd.card_next(&idx) == 0 && idx >= 0)
  {
    snd_ctl_t* ctl{};
    if(snd.ctl_open(&ctl, ("hw:" + std::to_string(idx)).c_str(), SND_CTL_NONBLOCK) < 0)
      continue;

    snd_ctl_card_info_t* info{};
    snd_alloca(&info, snd, ctl_card_info);
    if(snd.ctl_card_info(ctl, info) < 0)
    {
      snd.ctl_close(ctl);
      continue;
    }
    const QString id = snd.ctl_card_info_get_id(info);
    const QString card_name = snd.ctl_card_info_get_name(info);

    snd_pcm_info_t* pcm_info{};
    snd_alloca(&pcm_info, snd, pcm_info);
    int dev = -1;
    while(snd.ctl_pcm_next_device(ctl, &dev) == 0 && dev >= 0)
    {
      QString pcm_name;
      for(auto stream : {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE})
      {
        snd.pcm_info_set_device(pcm_info, dev);
        snd.pcm_info_set_subdevice(pcm_info, 0);
        snd.pcm_info_set_stream(pcm_info, stream);
        if(snd.ctl_pcm_info(ctl, pcm_info) == 0)
        {
          pcm_name = snd.pcm_info_get_name(pcm_info);
          break;
        }
      }

      const QString suffix = QString("CARD=%1,DEV=%2").arg(id).arg(dev);
      const QString pretty
          = pcm_name.isEmpty() ? card_name : card_name + ": " + pcm_name;
      add("hw:" + suffix, pretty, false);
      add("plughw:" + suffix, pretty, true);
    }
    snd.ctl_close(ctl);
  }

  // Whatever the system routes to, e.g. a sound server.
  add("default", tr("System default"), true);
}

QString ALSAFactory::prettyName() const
{
  return QObject::tr("ALSA (raw)");
}

std::shared_ptr<ossia::audio_engine> ALSAFactory::make_engine(
    const Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  return std::make_shared<ossia::alsa_engine>(
      set.getCardIn().toStdString(), set.getCardOut().toStdString(), set.getDefaultIn(),
      set.getDefaultOut(), set.getRate(), set.getBufferSize());
}

QWidget* ALSAFactory::make_settings(
    Audio::Settings::Model& m, Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp, QWidget* parent)
{
  if(!m_scanned)
    rescan();
  return new ALSAWidget{*this, m, v, m_disp, parent};
}

}
#endif
