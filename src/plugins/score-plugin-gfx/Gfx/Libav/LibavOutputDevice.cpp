#include "LibavOutputDevice.hpp"

#include <Gfx/Libav/LibavOutputStream.hpp>

#if SCORE_HAS_LIBAV
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Audio/Settings/Model.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/Libav/LibavEncoderNode.hpp>
#include <Media/LibavIntrospection.hpp>

#include <score/model/Skin.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSpinBox>

#include <wobjectimpl.h>

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::LibavOutputSettings);
namespace Gfx
{
// ffmpeg -y  -hwaccel cuda -hwaccel_output_format cuda -f video4linux2 -input_format mjpeg -framerate 30 -i  /dev/video0
// -fflags nobuffer
// -c:v hevc_nvenc
// -preset:v llhq
// -rc constqp
// -zerolatency 1
// -delay 0
// -forced-idr 1
// -g 1
// -cbr 1
// -qp 10
// -f matroska  -

// ffmpeg -y
//   -hwaccel cuda
//   -hwaccel_output_format cuda
//   -f video4linux2
//   -input_format mjpeg
//   -framerate 30
//   -i  /dev/video0
//   -fflags nobuffer
//   -c:v hevc_nvenc
//   -preset:v llhq
//   -rc constqp
//   -zerolatency 1
//   -delay 0
//   -forced-idr 1
//   -g 1
//   -cbr 1
//   -qp 10
//   -f matroska  -
// |
// ffplay
//   -probesize 32
//   -analyzeduration 0
//   -fflags nobuffer
//   -flags low_delay
//   -framedrop
//   -vf setpts=0
//   -sync ext -

class record_audio_parameter final : public ossia::audio_parameter
{
public:
  record_audio_parameter(
      LibavEncoder& encoder, int num_channels, int bs, ossia::net::node_base& n)
      : audio_parameter{n}
      , m_encoder{encoder}
      , m_audio_data(num_channels)
  {
    set_buffer_size(bs);
  }

  virtual ~record_audio_parameter() { }

  void push_value(const ossia::audio_port& mixed) noexcept override
  {
    m_audio_data.resize(mixed.channels());
    for(std::size_t i = 0; i < mixed.channels(); i++)
    {
      auto& chan = mixed.channel(i);
      m_audio_data[i].assign(chan.begin(), chan.end());
    }

    m_encoder.add_frame(m_audio_data);
  }

  void set_buffer_size(int bs)
  {
    const auto chan = m_audio_data.size();
    audio.resize(chan);
    for(std::size_t i = 0; i < chan; i++)
    {
      m_audio_data[i].resize(bs);
      audio[i] = m_audio_data[i];
      ossia::fill(m_audio_data[i], 0.f);
    }
  }

private:
  LibavEncoder& m_encoder;
  // todo use a flat vector instead for perf
  std::vector<ossia::float_vector> m_audio_data;
};
class libav_output_protocol : public Gfx::gfx_protocol_base
{
public:
  LibavEncoder encoder;
  explicit libav_output_protocol(const LibavOutputSettings& set, GfxExecutionAction& ctx)
      : gfx_protocol_base{ctx}
      , encoder{set}
  {
  }
  ~libav_output_protocol() { }

  void start_execution() override { encoder.start(); }
  void stop_execution() override { encoder.stop(); }
};

class libav_output_device : public ossia::net::device_base
{
  ossia::net::generic_node root;

public:
  libav_output_device(
      const LibavOutputSettings& set, LibavEncoder& enc,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{name, *this}
  {
    auto& p = *static_cast<gfx_protocol_base*>(m_protocol.get());
    auto node = new LibavEncoderNode{set, enc, 0};
    root.add_child(std::make_unique<gfx_node_base>(*this, p, node, "Video"));

    auto& audio_stgs = score::AppContext().settings<Audio::Settings::Model>();
    auto audio = root.add_child(
        std::make_unique<ossia::net::generic_node>("Audio", *this, root));
    audio->set_parameter(std::make_unique<record_audio_parameter>(
        enc, set.audio_channels, audio_stgs.getBufferSize(), *audio));
  }

  const ossia::net::generic_node& get_root_node() const override { return root; }
  ossia::net::generic_node& get_root_node() override { return root; }
};

class LibavOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(LibavOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~LibavOutputDevice();

private:
  void disconnect() override;
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<libav_output_device> m_dev;
};

class LibavOutputSettingsWidget final : public Gfx::SharedOutputSettingsWidget
{
public:
  explicit LibavOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;
  void loadPreset(const LibavOutputSettings& settings);

private:
  void on_presetChange(int index);
  void on_muxerChange(int index);
  void on_acodecChange(int index);
  void on_vcodecChange(int index);

  QComboBox* m_presets{};
  QComboBox* m_muxer{};
  QComboBox* m_vencoder{};
  QComboBox* m_aencoder{};
  QComboBox* m_pixfmt{};
  QComboBox* m_smpfmt{};
  QPlainTextEdit* m_options{};
  Device::DeviceSettings m_settings;
};

LibavOutputDevice::~LibavOutputDevice() { }

static const std::map<QString, LibavOutputSettings> libav_preset_list{
    // Play with:
    // ffplay -an -fflags nobuffer -flags low_delay -probesize 32 -analyzeduration 1 -strict experimental -framedrop -vf setpts=0  'udp://127.0.0.1:1234'
    {"UDP MJPEG streaming",
     LibavOutputSettings{
         .path = "udp://192.168.1.80:8081",
         .width = 1280,
         .height = 720,
         .rate = 30,
         .audio_encoder_short = {},
         .audio_encoder_long = {},
         .audio_converted_smpfmt = {},
         .video_encoder_short = "mjpeg",
         .video_encoder_long = {},
         .video_render_pixfmt = "rgba",
         .video_converted_pixfmt = "yuv420p",
         .muxer = "mjpeg",
         .muxer_long = {},
         .options
         = {{"fflags", "+nobuffer+genpts"},
            {"flags", "+low_delay"},
            {"flush_packets", "1"}},
         .threads = 0}},

    {"MKV H.264 recording",
     LibavOutputSettings{
         .path = "<PROJECT_PATH>/main.mkv",
         .width = 1280,
         .height = 720,
         .rate = 30,
         .audio_encoder_short = {},
         .audio_encoder_long = {},
         .audio_converted_smpfmt = {},
         .video_encoder_short = "libx265",
         .video_encoder_long = {},
         .video_render_pixfmt = "rgba",
         .video_converted_pixfmt = "yuv420p",
         .muxer = "matroska",
         .muxer_long = {},
         .options = {},
         .threads = 0}},

    // clang-format off
    // Read with:
    // ffplay -an -fflags nobuffer -flags low_delay -probesize 32 -analyzeduration 1 -strict experimental -framedrop -vf setpts=0  'srt://127.0.0.1:40052?mode=caller
    {"SRT streaming",
     LibavOutputSettings{
                         .path = "srt://:40052?mode=listener&latency=2000&transtype=live&recv_buffer_size=0",
                         .width = 1280,
                         .height = 720,
                         .rate = 30,
                         .audio_encoder_short = {},
                         .audio_encoder_long = {},
                         .audio_converted_smpfmt = {},
                         .video_encoder_short = "libx264",
                         .video_encoder_long = {},
                         .video_render_pixfmt = "rgba",
                         .video_converted_pixfmt = "yuv420p",
                         .muxer = "mpegts",
                         .muxer_long = {},
                         .options = { {"preset", "ultrafast"}, {"tune", "zerolatency"}, {"flush_packets", "1"}},
                         .threads = 0}},
    // clang-format on

    {"WAV recording",
     LibavOutputSettings{
         .path = "<PROJECT_PATH>/main.wav",
         .width = 1280,
         .height = 720,
         .rate = 30,
         .audio_encoder_short = "pcm_s16le",
         .audio_encoder_long = "",
         .audio_converted_smpfmt = {},
         .video_encoder_short = "",
         .video_encoder_long = "",
         .video_render_pixfmt = "",
         .video_converted_pixfmt = "",
         .muxer = "wav",
         .muxer_long = {},
         .options = {},
         .threads = 0,
     }}

    // av_dict_set(&opt, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
};

void LibavOutputDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool LibavOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<LibavOutputSettings>();
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto m_protocol = new libav_output_protocol{set, plug->exec};
      m_dev = std::make_unique<libav_output_device>(
          set, m_protocol->encoder, std::unique_ptr<libav_output_protocol>(m_protocol),
          this->settings().name.toStdString());
      deviceChanged(nullptr, m_dev.get());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

QString LibavOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Libav Output");
}

QString LibavOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::util;
}

QUrl LibavOutputProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/libav-device.html");
}

Device::DeviceInterface* LibavOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LibavOutputDevice(settings, ctx);
}

const Device::DeviceSettings&
LibavOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Libav Output";
    LibavOutputSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* LibavOutputProtocolFactory::makeSettingsWidget()
{
  return new LibavOutputSettingsWidget;
}

QVariant LibavOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LibavOutputSettings>(visitor);
}

void LibavOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LibavOutputSettings>(data, visitor);
}

bool LibavOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

LibavOutputSettingsWidget::LibavOutputSettingsWidget(QWidget* parent)
    : SharedOutputSettingsWidget{parent}
{
  m_deviceNameEdit->setText("Libav Out");
  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Path");

  const auto& info = LibavIntrospection::instance();

  m_presets = new QComboBox{};
  {
    for(auto& [name, preset] : libav_preset_list)
      m_presets->addItem(name);
  }

  m_muxer = new QComboBox{};
  for(auto& mux : info.muxers)
  {
    QString name = mux.format->name;
    if(mux.format->long_name && strlen(mux.format->long_name) > 0)
    {
      name += " (";
      name += mux.format->long_name;
      name += ")";
    }
    m_muxer->addItem(name, QVariant::fromValue((void*)mux.format));
  }

  m_vencoder = new QComboBox{};
  m_aencoder = new QComboBox{};
  m_pixfmt = new QComboBox{};
  m_smpfmt = new QComboBox{};
  m_options = new QPlainTextEdit{};
  m_options->setFont(score::Skin::instance().MonoFontSmall);

  connect(
      m_presets, &QComboBox::currentIndexChanged, this,
      &LibavOutputSettingsWidget::on_presetChange);
  connect(
      m_muxer, &QComboBox::currentIndexChanged, this,
      &LibavOutputSettingsWidget::on_muxerChange);
  connect(
      m_vencoder, &QComboBox::currentIndexChanged, this,
      &LibavOutputSettingsWidget::on_vcodecChange);
  connect(
      m_aencoder, &QComboBox::currentIndexChanged, this,
      &LibavOutputSettingsWidget::on_acodecChange);

  m_layout->addRow(tr("Preset"), m_presets);
  m_layout->addRow(tr("Muxer"), m_muxer);
  m_layout->addRow(tr("Video encoder"), m_vencoder);
  m_layout->addRow(tr("Pixel format"), m_pixfmt);
  m_layout->addRow(tr("Audio encoder"), m_aencoder);
  m_layout->addRow(tr("Sample format"), m_smpfmt);
  m_layout->addRow(tr("Additional options"), m_options);

  m_width->setValue(1280);
  m_height->setValue(720);
  m_rate->setValue(30);
}

void LibavOutputSettingsWidget::on_presetChange(int preset_index)
{
  auto name = m_presets->itemText(preset_index);
  auto it = libav_preset_list.find(name);
  if(it != libav_preset_list.end())
  {
    loadPreset(it->second);
  }
}

void LibavOutputSettingsWidget::on_muxerChange(int muxer_index)
{
  m_vencoder->clear();
  m_aencoder->clear();
  m_pixfmt->clear();
  m_smpfmt->clear();
  const auto& info = LibavIntrospection::instance();
  if(muxer_index < 0 || muxer_index >= info.muxers.size())
    return;

  auto& mux = info.muxers[muxer_index];
  const AVCodec* default_vcodec = nullptr;
  const AVCodec* default_acodec = nullptr;
  for(auto& vcodec : mux.vcodecs)
  {
    QString name = vcodec->codec->name;
    if(vcodec->codec->long_name && strlen(vcodec->codec->long_name) > 0)
    {
      name += " (";
      name += vcodec->codec->long_name;
      name += ")";
    }
    if(!default_vcodec && mux.format->video_codec == vcodec->codec->id)
      default_vcodec = vcodec->codec;
    m_vencoder->addItem(name, QVariant::fromValue((void*)vcodec->codec));
  }

  for(auto& acodec : mux.acodecs)
  {
    QString name = acodec->codec->name;
    if(acodec->codec->long_name && strlen(acodec->codec->long_name) > 0)
    {
      name += " (";
      name += acodec->codec->long_name;
      name += ")";
    }
    if(!default_acodec && mux.format->audio_codec == acodec->codec->id)
      default_acodec = acodec->codec;
    m_aencoder->addItem(name, QVariant::fromValue((void*)acodec->codec));
  }

  if(default_vcodec)
  {
    int vc_index = m_vencoder->findData(QVariant::fromValue((void*)default_vcodec));
    if(vc_index != -1)
      m_vencoder->setCurrentIndex(vc_index);
  }
  if(default_acodec)
  {
    int ac_index = m_aencoder->findData(QVariant::fromValue((void*)default_acodec));
    if(ac_index != -1)
      m_aencoder->setCurrentIndex(ac_index);
  }
}

void LibavOutputSettingsWidget::on_vcodecChange(int codec_index)
{
  m_pixfmt->clear();
  const auto& info = LibavIntrospection::instance();
  int muxer_index = m_muxer->currentIndex();
  if(muxer_index < 0 || muxer_index >= info.muxers.size())
    return;

  auto& mux = info.muxers[muxer_index];
  if(codec_index < 0 || codec_index >= mux.vcodecs.size())
    return;

  auto codec = mux.vcodecs[codec_index];

  const AVPixelFormat* supported_pixfmts{};
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
  avcodec_get_supported_config(
      nullptr, codec->codec, AV_CODEC_CONFIG_PIX_FORMAT, 0,
      (const void**)&supported_pixfmts, nullptr);
#else
  supported_pixfmts = codec->codec->pix_fmts;
#endif
  if(!supported_pixfmts)
    return;

  for(auto fmt = supported_pixfmts; *fmt != AV_PIX_FMT_NONE; ++fmt)
  {
    if(auto desc = av_pix_fmt_desc_get(*fmt))
      m_pixfmt->addItem(desc->name);
  }
}

void LibavOutputSettingsWidget::on_acodecChange(int codec_index)
{
  m_smpfmt->clear();
  const auto& info = LibavIntrospection::instance();
  int muxer_index = m_muxer->currentIndex();
  if(muxer_index < 0 || muxer_index >= info.muxers.size())
    return;

  auto& mux = info.muxers[muxer_index];
  if(codec_index < 0 || codec_index >= mux.acodecs.size())
    return;

  auto codec = mux.acodecs[codec_index];
  const AVSampleFormat* supported_sample_fmts{};

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
  avcodec_get_supported_config(
      nullptr, codec->codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
      (const void**)&supported_sample_fmts, nullptr);
#else
  supported_sample_fmts = codec->codec->sample_fmts;
#endif
  if(!supported_sample_fmts)
    return;
  for(auto fmt = supported_sample_fmts; *fmt != AV_SAMPLE_FMT_NONE; ++fmt)
  {
    if(auto desc = av_get_sample_fmt_name(*fmt))
      m_smpfmt->addItem(desc);
  }
}

Device::DeviceSettings LibavOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = SharedOutputSettingsWidget::getSettings();
  s.protocol = LibavOutputProtocolFactory::static_concreteKey();
  const auto& base_s = s.deviceSpecificSettings.value<SharedOutputSettings>();
  LibavOutputSettings specif{
      .path = base_s.path,
      .width = base_s.width,
      .height = base_s.height,
      .rate = base_s.rate,
      .audio_encoder_short = {},
      .audio_encoder_long = {},
      .audio_converted_smpfmt = {},
      .video_encoder_short = {},
      .video_encoder_long = {},
      .video_render_pixfmt = {},
      .video_converted_pixfmt = {},
      .muxer = {},
      .muxer_long = {},
      .options = {},
      .threads = {},
  };

  auto muxer = (AVOutputFormat*)this->m_muxer->currentData().value<void*>();
  auto vcodec = (AVCodec*)this->m_vencoder->currentData().value<void*>();
  auto acodec = (AVCodec*)this->m_aencoder->currentData().value<void*>();
  if(muxer)
  {
    specif.muxer = muxer->name;
    if(muxer->long_name)
      specif.muxer_long = muxer->long_name;

    if(acodec)
    {
      specif.audio_encoder_short = acodec->name;
      if(acodec->long_name)
        specif.audio_encoder_long = acodec->long_name;
      specif.audio_converted_smpfmt = this->m_smpfmt->currentText();
    }

    if(vcodec)
    {
      specif.video_encoder_short = vcodec->name;
      if(vcodec->long_name)
        specif.video_encoder_long = vcodec->long_name;
      
      specif.video_render_pixfmt = av_pix_fmt_desc_get(AV_PIX_FMT_RGBA)->name;
      specif.video_converted_pixfmt = this->m_pixfmt->currentText();
    }

    specif.hardwareAcceleration = {};
    specif.threads = 0;

    {
      auto split = this->m_options->toPlainText().split('\n', Qt::SkipEmptyParts);

      for(auto line : split)
      {
        line = line.trimmed();
        if(!line.startsWith('-'))
          continue;
        line.remove(0, 1);

        auto first_space = line.indexOf(' ');
        if(first_space == -1)
        {
          specif.options.emplace(line, "");
        }
        else
        {
          auto key = line.mid(0, first_space).trimmed();
          auto v = line.mid(first_space).trimmed();
          specif.options.emplace(key, v);
        }
      }
    }
  }
  s.deviceSpecificSettings = QVariant::fromValue(std::move(specif));
  return s;
}

void LibavOutputSettingsWidget::loadPreset(const LibavOutputSettings& settings)
{
  const auto& info = LibavIntrospection::instance();

  m_shmPath->setText(settings.path);
  m_width->setValue(settings.width);
  m_height->setValue(settings.height);
  m_rate->setValue(settings.rate);

  auto muxer = info.findMuxer(settings.muxer, settings.muxer_long);
  if(!muxer)
    return;

  auto vcodec
      = info.findVideoCodec(settings.video_encoder_short, settings.video_encoder_long);
  auto acodec
      = info.findAudioCodec(settings.audio_encoder_short, settings.audio_encoder_long);

  if(!vcodec && !acodec)
    return;

  {
    int mux_index = m_muxer->findData(QVariant::fromValue((void*)muxer->format));
    if(mux_index != -1)
      m_muxer->setCurrentIndex(mux_index);
  }
  if(vcodec)
  {
    int vc_index = m_vencoder->findData(QVariant::fromValue((void*)vcodec->codec));
    if(vc_index != -1)
      m_vencoder->setCurrentIndex(vc_index);
  }
  if(acodec)
  {
    int ac_index = m_aencoder->findData(QVariant::fromValue((void*)acodec->codec));
    if(ac_index != -1)
      m_aencoder->setCurrentIndex(ac_index);
  }

  QString options_text;

  for(auto [k, v] : settings.options)
  {
    options_text += '-';
    options_text += k;
    options_text += ' ';
    options_text += v;
    options_text += '\n';
  }
  m_options->setPlainText(options_text);
}

void LibavOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);

  const auto& set = settings.deviceSpecificSettings.value<LibavOutputSettings>();
  loadPreset(set);
}
}

template <>
void DataStreamReader::read(const Gfx::LibavOutputSettings& n)
{
  m_stream << n.path << n.width << n.height << n.rate;

  m_stream << n.hardwareAcceleration;
  m_stream << n.audio_encoder_short << n.audio_encoder_long << n.audio_converted_smpfmt
           << n.audio_sample_rate << n.audio_channels;
  m_stream << n.video_encoder_short << n.video_encoder_long;
  m_stream << n.video_render_pixfmt;
  m_stream << n.video_converted_pixfmt;
  m_stream << n.muxer << n.muxer_long;
  m_stream << n.options;
  m_stream << n.threads;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::LibavOutputSettings& n)
{
  m_stream >> n.path >> n.width >> n.height >> n.rate;

  m_stream >> n.hardwareAcceleration;

  m_stream >> n.audio_encoder_short >> n.audio_encoder_long >> n.audio_converted_smpfmt
      >> n.audio_sample_rate >> n.audio_channels;

  m_stream >> n.video_encoder_short >> n.video_encoder_long;
  m_stream >> n.video_render_pixfmt;
  m_stream >> n.video_converted_pixfmt;

  m_stream >> n.muxer >> n.muxer_long;

  m_stream >> n.options;
  m_stream >> n.threads;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::LibavOutputSettings& n)
{
  obj["Path"] = n.path;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Rate"] = n.rate;

  obj["HWAccel"] = n.hardwareAcceleration;

  obj["AudioEncoderShort"] = n.audio_encoder_short;
  obj["AudioEncoderLong"] = n.audio_encoder_long;
  obj["AudioConvSmpFmt"] = n.audio_converted_smpfmt;
  obj["AudioRate"] = n.audio_sample_rate;
  obj["AudioChannels"] = n.audio_channels;

  obj["VideoEncoderShort"] = n.video_encoder_short;
  obj["VideoEncoderLong"] = n.video_encoder_long;
  obj["VideoRenderPixFmt"] = n.video_render_pixfmt;
  obj["VideoConvPixFmt"] = n.video_converted_pixfmt;

  obj["MuxerShort"] = n.muxer;
  obj["MuxerLong"] = n.muxer_long;

  obj["Options"] = n.options;
  obj["Threads"] = n.threads;
}

template <>
void JSONWriter::write(Gfx::LibavOutputSettings& n)
{
  n.path = obj["Path"].toString();
  n.width = obj["Width"].toDouble();
  n.height = obj["Height"].toDouble();
  n.rate = obj["Rate"].toDouble();

  n.hardwareAcceleration <<= obj["HWAccel"];

  n.audio_encoder_short <<= obj["AudioEncoderShort"];
  n.audio_encoder_long <<= obj["AudioEncoderLong"];
  n.audio_converted_smpfmt <<= obj["AudioConvSmpFmt"];
  n.audio_sample_rate <<= obj["AudioRate"];
  n.audio_channels <<= obj["AudioChannels"];

  n.video_encoder_short <<= obj["VideoEncoderShort"];
  n.video_encoder_long <<= obj["VideoEncoderLong"];
  n.video_render_pixfmt <<= obj["VideoRenderPixFmt"];
  n.video_converted_pixfmt <<= obj["VideoConvPixFmt"];

  n.muxer <<= obj["MuxerShort"];
  n.muxer_long <<= obj["MuxerLong"];

  n.options <<= obj["Options"];
  n.threads <<= obj["Threads"];
}

W_OBJECT_IMPL(Gfx::LibavOutputDevice)
#endif
