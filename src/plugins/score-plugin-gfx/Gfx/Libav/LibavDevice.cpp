#include "LibavDevice.hpp"

#if SCORE_HAS_LIBAV
#include <State/MessageListSerialization.hpp>

#include <Audio/Settings/Model.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/Libav/LibavEncoderNode.hpp>
#include <Gfx/Libav/LibavOutputStream.hpp>
#include <Gfx/Libav/LibavSettingsWidget.hpp>
#include <Media/LibavIntrospection.hpp>
#include <Video/LibavStreamInput.hpp>

#include <score/serialization/MapSerialization.hpp>
#include <score/serialization/MimeVisitor.hpp>
#include <score/tools/FilePath.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/name_utils.hpp>

#include <wobjectimpl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

namespace Gfx
{
// Audio parameter that reads from LibavStreamInput's AudioRingBuffer
class libav_input_audio_parameter final : public ossia::audio_parameter
{
public:
  libav_input_audio_parameter(
      Video::AudioRingBuffer& buf, int num_channels, int bs, ossia::net::node_base& n)
      : audio_parameter{n}
      , m_buffer{buf}
      , m_audio_data(num_channels)
  {
    audio.resize(num_channels);
    for(int i = 0; i < num_channels; i++)
    {
      m_audio_data[i].resize(bs, 0.f);
      audio[i] = m_audio_data[i];
    }
    m_buffer.output_data = &m_audio_data;
  }

  virtual ~libav_input_audio_parameter() { m_buffer.output_data = nullptr; }

  void pull_value() override
  {
    // Not called for device params, but implement for completeness
  }

  void refresh_from_ring()
  {
    // Called indirectly via pre_tick
    for(std::size_t i = 0; i < m_audio_data.size(); i++)
      audio[i] = m_audio_data[i];
  }

private:
  Video::AudioRingBuffer& m_buffer;
  std::vector<ossia::float_vector> m_audio_data;
};

// Resolve file paths (<PROJECT>:, <LIBRARY>:, relative) but leave URLs and
// filter graphs (lavfi) untouched.
static QString resolveLibavPath(
    const QString& path, const ossia::hash_map<QString, QString>& options,
    const score::DocumentContext& ctx)
{
  if(path.contains("://"))
    return path;

  auto it = options.find(QStringLiteral("format"));
  if(it != options.end() && it->second == QStringLiteral("lavfi"))
    return path;

  auto resolved = score::locateFilePath(path, ctx);
  return resolved.isEmpty() ? path : resolved;
}

// Protocol for FFmpeg input
class libav_input_protocol : public ossia::net::protocol_base
{
public:
  std::shared_ptr<Video::LibavStreamInput> stream;
  ossia::net::parameter_base* path_param{};

  // Stored so we can reload with the same options when the path changes at runtime
  ossia::hash_map<QString, QString> m_opts;
  std::map<std::string, std::string> m_std_opts;
  const score::DocumentContext& m_ctx;

  libav_input_protocol(
      const std::string& url, const ossia::hash_map<QString, QString>& opts,
      const score::DocumentContext& ctx)
      : ossia::net::protocol_base{flags{}}
      , stream{std::make_shared<Video::LibavStreamInput>()}
      , m_opts{opts}
      , m_ctx{ctx}
  {
    for(const auto& [k, v] : opts)
      m_std_opts[k.toStdString()] = v.toStdString();

    load_and_probe(url);
  }

  void load_and_probe(const std::string& url)
  {
    if(m_std_opts.empty())
      stream->load(url);
    else
      stream->load(url, m_std_opts);

    // Probe the stream to discover width/height/audio metadata.
    // This opens the format context and keeps it alive for later start().
    stream->probe();
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

  std::string resolvePath(const std::string& path) const
  {
    return resolveLibavPath(QString::fromStdString(path), m_opts, m_ctx).toStdString();
  }

  void start_execution() override
  {
    // Read the current path from the parameter (user may have changed it
    // in the device explorer before hitting play)
    if(path_param)
    {
      try
      {
        auto v = path_param->value();
        if(auto s = v.target<std::string>())
        {
          if(!s->empty())
          {
            stream->stop();
            load_and_probe(resolvePath(*s));
          }
        }
      }
      catch(...)
      {
      }
    }

    stream->start();
  }
  void stop_execution() override { stream->stop(); }

  void pre_tick(std::size_t buffer_size) override
  {
    if(stream->has_audio())
      stream->audio_buffer().read_into_output(buffer_size);
  }
};

// Device tree for FFmpeg input
class libav_input_device : public ossia::net::device_base
{
  ossia::net::generic_node root;

public:
  libav_input_device(
      GfxExecutionAction& ctx, std::unique_ptr<libav_input_protocol> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{name, *this}
  {
    m_capabilities.change_tree = true;
    auto* p = static_cast<libav_input_protocol*>(m_protocol.get());
    auto& input = p->stream;

    // Video output node
    if(input->width > 0 && input->height > 0)
    {
      root.add_child(
          std::make_unique<Gfx::simple_texture_input_node>(
              new score::gfx::CameraNode(input), &ctx, *this, "Video"));
    }

    // Audio output node
    if(input->has_audio())
    {
      auto& audio_stgs = score::AppContext().settings<Audio::Settings::Model>();
      auto audio_node = std::make_unique<ossia::net::generic_node>("Audio", *this, root);
      audio_node->set_parameter(
          std::make_unique<libav_input_audio_parameter>(
              input->audio_buffer(), input->audio_buffer().num_channels,
              audio_stgs.getBufferSize(), *audio_node));
      root.add_child(std::move(audio_node));
    }

    // Expose path as a controllable parameter:
    // Setting it during execution stops the current stream and opens the new one.
    {
      auto path_node = std::make_unique<ossia::net::generic_node>("path", *this, root);
      auto* param = path_node->create_parameter(ossia::val_type::STRING);
      if(param)
      {
        param->set_access(ossia::access_mode::BI);
        param->push_value(input->url());

        p->path_param = param;

        param->add_callback([p](const ossia::value& v) {
          if(auto val = v.target<std::string>())
          {
            // Stop current stream and reload with the new path
            p->stream->stop();

            if(!val->empty())
            {
              p->load_and_probe(p->resolvePath(*val));
              p->stream->start();
            }
          }
        });
      }
      root.add_child(std::move(path_node));
    }
  }

  const ossia::net::generic_node& get_root_node() const override { return root; }
  ossia::net::generic_node& get_root_node() override { return root; }
};

// Audio parameter that receives audio from the engine and sends to encoder
class libav_record_audio_parameter final : public ossia::audio_parameter
{
public:
  libav_record_audio_parameter(
      LibavEncoder& encoder, int num_channels, int bs, ossia::net::node_base& n)
      : audio_parameter{n}
      , m_encoder{encoder}
      , m_audio_data(num_channels)
  {
    audio.resize(num_channels);
    for(int i = 0; i < num_channels; i++)
    {
      m_audio_data[i].resize(bs, 0.f);
      audio[i] = m_audio_data[i];
    }
  }

  virtual ~libav_record_audio_parameter() { }

  void push_value(const ossia::audio_port& mixed) noexcept override
  {
    if(!m_encoder.available())
      return;

    m_audio_data.resize(mixed.channels());
    for(std::size_t i = 0; i < mixed.channels(); i++)
    {
      auto& chan = mixed.channel(i);
      m_audio_data[i].assign(chan.begin(), chan.end());
    }

    m_encoder.add_frame(m_audio_data);
  }

private:
  LibavEncoder& m_encoder;
  std::vector<ossia::float_vector> m_audio_data;
};

// Protocol for FFmpeg output
class libav_output_protocol : public Gfx::gfx_protocol_base
{
public:
  LibavEncoder encoder;
  ossia::net::parameter_base* path_param{};
  const score::DocumentContext& m_ctx;

  explicit libav_output_protocol(
      const LibavOutputSettings& set, GfxExecutionAction& ctx,
      const score::DocumentContext& docCtx)
      : gfx_protocol_base{ctx}
      , encoder{set}
      , m_ctx{docCtx}
  {
  }

  ~libav_output_protocol() { }

  void start_execution() override
  {
    // Read the current path from the parameter (user may have changed it
    // in the device explorer before hitting play)
    if(path_param)
    {
      try
      {
        auto v = path_param->value();
        if(auto s = v.target<std::string>())
        {
          auto resolved = resolveLibavPath(
              QString::fromStdString(*s), {}, m_ctx);
          encoder.m_set.path = resolved;
        }
      }
      catch(...)
      {
      }
    }

    if(!encoder.m_set.path.isEmpty())
      encoder.start();
  }
  void stop_execution() override { encoder.stop(); }
};

// Device tree for FFmpeg output
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
    auto& out_proto = *static_cast<libav_output_protocol*>(m_protocol.get());
    auto node = new LibavEncoderNode{set, enc, 0};
    root.add_child(std::make_unique<gfx_node_base>(*this, p, node, "Video"));

    if(set.audio_channels > 0 && !set.audio_encoder_short.isEmpty())
    {
      auto& audio_stgs = score::AppContext().settings<Audio::Settings::Model>();
      auto audio = root.add_child(
          std::make_unique<ossia::net::generic_node>("Audio", *this, root));
      audio->set_parameter(
          std::make_unique<libav_record_audio_parameter>(
              enc, set.audio_channels, audio_stgs.getBufferSize(), *audio));
    }

    // Expose path as a controllable parameter:
    // Setting it during execution starts/stops/changes the output file.
    {
      auto path_node = std::make_unique<ossia::net::generic_node>("path", *this, root);
      auto* param = path_node->create_parameter(ossia::val_type::STRING);
      if(param)
      {
        param->set_access(ossia::access_mode::BI);
        param->push_value(set.path.toStdString());

        out_proto.path_param = param;

        param->add_callback([&enc, &out_proto](const ossia::value& v) {
          if(auto val = v.target<std::string>())
          {
            // Hold the mux mutex to prevent concurrent add_frame during stop/start
            std::lock_guard lock{enc.m_muxMutex};

            // Stop current encoding (use stop_impl — lock already held)
            enc.stop_impl();

            if(!val->empty())
            {
              // Resolve path and restart
              enc.m_set.path = resolveLibavPath(
                  QString::fromStdString(*val), {}, out_proto.m_ctx);
              enc.start();
            }
          }
        });
      }
      root.add_child(std::move(path_node));
    }
  }

  const ossia::net::generic_node& get_root_node() const override { return root; }
  ossia::net::generic_node& get_root_node() override { return root; }
};

// Unified Score-facing device (handles both Input and Output based on direction)
class LibavDevice final : public Gfx::GfxOutputDevice
{
  W_OBJECT(LibavDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~LibavDevice();

private:
  void disconnect() override;
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

W_OBJECT_IMPL(Gfx::LibavDevice)

LibavDevice::~LibavDevice() { }

void LibavDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  if(prev)
    deviceChanged(prev.get(), nullptr);
}

bool LibavDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<LibavSettings>();
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(!plug)
      return false;

    // Resolve <PROJECT>:, <LIBRARY>:, and relative paths before passing to FFmpeg.
    // URLs (rtsp://, srt://, etc.) and filter graphs (lavfi) are left as-is.
    auto resolvedPath = resolveLibavPath(set.path, set.options, m_ctx);

    if(set.direction == LibavSettings::Input)
    {
      auto proto = std::make_unique<libav_input_protocol>(
          resolvedPath.toStdString(), set.options, m_ctx);
      m_dev = std::make_unique<libav_input_device>(
          plug->exec, std::move(proto), this->settings().name.toStdString());
    }
    else
    {
      auto outSet = set.toOutputSettings();
      outSet.path = resolvedPath;
      auto proto = new libav_output_protocol{outSet, plug->exec, m_ctx};
      m_dev = std::make_unique<libav_output_device>(
          outSet, proto->encoder, std::unique_ptr<libav_output_protocol>(proto),
          this->settings().name.toStdString());
    }
    deviceChanged(nullptr, m_dev.get());
  }
  catch(std::exception& e)
  {
    qDebug() << "FFmpeg: Could not connect:" << e.what();
  }
  catch(...)
  {
  }

  return connected();
}

// ============================================================================
// PROTOCOL FACTORY
// ============================================================================

QString LibavProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("FFmpeg");
}

QString LibavProtocolFactory::category() const noexcept
{
  return StandardCategories::media;
}

QUrl LibavProtocolFactory::manual() const noexcept
{
  return {};
}

Device::DeviceInterface* LibavProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LibavDevice(settings, ctx);
}

const Device::DeviceSettings& LibavProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "FFmpeg";
    LibavSettings specif;
    specif.direction = LibavSettings::Input;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* LibavProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* LibavProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* LibavProtocolFactory::makeSettingsWidget()
{
  return new LibavSettingsWidget;
}

QVariant
LibavProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LibavSettings>(visitor);
}

void LibavProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LibavSettings>(data, visitor);
}

bool LibavProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}

// ============================================================================
// SERIALIZATION
// ============================================================================

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::LibavSettings);

template <>
void DataStreamReader::read(const Gfx::LibavSettings& n)
{
  m_stream << (int)n.direction << n.path << n.width << n.height << n.rate
           << n.audio_channels << n.threads << n.audio_encoder_short
           << n.audio_encoder_long << n.audio_converted_smpfmt << n.audio_sample_rate
           << n.video_encoder_short << n.video_encoder_long << n.video_render_pixfmt
           << n.video_converted_pixfmt << n.muxer << n.muxer_long << n.options
           << n.input_transfer;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::LibavSettings& n)
{
  int dir{};
  m_stream >> dir >> n.path >> n.width >> n.height >> n.rate >> n.audio_channels
      >> n.threads >> n.audio_encoder_short >> n.audio_encoder_long
      >> n.audio_converted_smpfmt >> n.audio_sample_rate >> n.video_encoder_short
      >> n.video_encoder_long >> n.video_render_pixfmt >> n.video_converted_pixfmt
      >> n.muxer >> n.muxer_long >> n.options >> n.input_transfer;
  n.direction = (Gfx::LibavSettings::Direction)dir;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::LibavSettings& n)
{
  obj["Direction"] = (int)n.direction;
  obj["Path"] = n.path;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Rate"] = n.rate;
  obj["AudioChannels"] = n.audio_channels;
  obj["Threads"] = n.threads;
  obj["AudioEncoderShort"] = n.audio_encoder_short;
  obj["AudioEncoderLong"] = n.audio_encoder_long;
  obj["AudioSmpFmt"] = n.audio_converted_smpfmt;
  obj["AudioSampleRate"] = n.audio_sample_rate;
  obj["VideoEncoderShort"] = n.video_encoder_short;
  obj["VideoEncoderLong"] = n.video_encoder_long;
  obj["VideoRenderPixFmt"] = n.video_render_pixfmt;
  obj["VideoConvertedPixFmt"] = n.video_converted_pixfmt;
  obj["Muxer"] = n.muxer;
  obj["MuxerLong"] = n.muxer_long;
  obj["Options"] = n.options;
  obj["InputTransfer"] = n.input_transfer;
}

template <>
void JSONWriter::write(Gfx::LibavSettings& n)
{
  n.direction = (Gfx::LibavSettings::Direction)obj["Direction"].toInt();
  n.path = obj["Path"].toString();
  n.width = obj["Width"].toInt();
  n.height = obj["Height"].toInt();
  n.rate = obj["Rate"].toDouble();
  n.audio_channels = obj["AudioChannels"].toInt();
  n.threads = obj["Threads"].toInt();
  n.audio_encoder_short = obj["AudioEncoderShort"].toString();
  n.audio_encoder_long = obj["AudioEncoderLong"].toString();
  n.audio_converted_smpfmt = obj["AudioSmpFmt"].toString();
  n.audio_sample_rate = obj["AudioSampleRate"].toInt();
  n.video_encoder_short = obj["VideoEncoderShort"].toString();
  n.video_encoder_long = obj["VideoEncoderLong"].toString();
  n.video_render_pixfmt = obj["VideoRenderPixFmt"].toString();
  n.video_converted_pixfmt = obj["VideoConvertedPixFmt"].toString();
  n.muxer = obj["Muxer"].toString();
  n.muxer_long = obj["MuxerLong"].toString();
  n.options <<= obj["Options"];
  if(auto v = obj.tryGet("InputTransfer"))
    n.input_transfer = v->toInt();
}

#endif
