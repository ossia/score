#include "ShmdataInputDevice.hpp"

#include <ossia/detail/flicks.hpp>

#include <QDebug>
#include <QElapsedTimer>

#include <fmt/format.h>

#include <functional>

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>
#include <Video/Rescale.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <shmdata/reader.hpp>
#include <shmdata/console-logger.hpp>

#include <Video/FrameQueue.hpp>
#include <Video/GpuFormats.hpp>
#include <Video/GStreamerCompatibility.hpp>

#include <Video/VideoInterface.hpp>

extern "C"
{
#include <libavformat/avformat.h>
}

W_OBJECT_IMPL(Gfx::Shmdata::InputDevice)

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::Shmdata::InputSettings);

namespace Gfx::Shmdata
{

class InputStream final : public ::Video::ExternalInput
{
public:
  explicit InputStream(const QString& path) noexcept
    : m_path{path.toStdString()}
    , m_receiver{std::in_place_t{},
        m_path,
        [this] (void* p, size_t sz) { on_data(p, sz); },
        [this] (const std::string& s) { setup(s); },
        [] () {},
        &m_logger}
  {
    realTime = true;
  }

  ~InputStream() noexcept
  {
    stop();
  }

  bool start() noexcept override
  {
    if (m_running)
      return false;

    if(!m_receiver)
    {
      m_receiver.emplace(
              m_path,
              [this] (void* p, size_t sz) { on_data(p, sz); },
              [this] (const std::string& s) { setup(s); },
              [] () {},
              &m_logger);
    }

    m_running.store(true, std::memory_order_release);
    return true;
  }

  void stop() noexcept override
  {
    // Stop the running status
    m_running.store(false, std::memory_order_release);

    m_receiver.reset();

    // Remove frames that were in flight
    m_frames.drain();
  }


  AVFrame* dequeue_frame() noexcept override
  {
    return m_frames.dequeue();
  }

  void release_frame(AVFrame* frame) noexcept override
  {
    m_frames.release(frame);
  }

private:
  void setup(const std::string& str)
  {
    // "video/x-raw, format=(string)AYUV64, width=(int)320, height=(int)240, framerate=(fraction)30/1, multiview-mode=(string)mono, pixel-aspect-ratio=(fraction)1/1, interlace-mode=(string)progressive";

    static const QRegularExpression parens{"\\(.*\\)"};
    auto split = QString::fromStdString(str).split(",");
    qDebug() << "Shmdata Input format:" << split;
    bool is_video = !split.empty() && split.front() == "video/x-raw";
    if(!is_video)
      return;

    QString format;
    int w = 0;
    int h = 0;
    double rate = 0.;
    for(auto& elt : split) {
      elt = elt.trimmed();
      if(elt.startsWith("format="))
      {
        elt.remove("format=");
        elt.remove(parens);
        format = elt;
      }
      else if(elt.startsWith("width"))
      {
        elt.remove("width=");
        elt.remove(parens);
        w = elt.toInt();
      }
      else if(elt.startsWith("height="))
      {
        elt.remove("height=");
        elt.remove(parens);
        h = elt.toInt();
      }
      else if(elt.startsWith("framerate="))
      {
        elt.remove("framerate=");
        if(elt.startsWith("(fraction)"))
        {
          elt.remove("(fraction)");
          auto parts = elt.split("/");
          if(parts.size() == 2) {
            double num = parts[0].toDouble();
            double denom = parts[1].toDouble();
            if(denom > 0) {
              rate = num / denom;
            }
          }
        }
        else
        {
          elt.remove(parens);
          rate = elt.toDouble();
        }
      }
    }

    if(format.isEmpty() || w < 1 || h < 1 || rate < 1)
    {
      return;
    }

    const auto& fmts = ::Video::gstreamerToLibav();
    if(auto it = fmts.find(format.toUpper().toStdString()); it != fmts.end())
    {
      qDebug() << "ShmdataInput: supported format" << format;
      this->pixel_format = it->second;
    }
    else
    {
      qDebug() << "ShmdataInput: unhandled format" << format;
      return;
    }
    this->width = w;
    this->height = h;

    if(::Video::formatNeedsDecoding(pixel_format))
    {
      m_rescale.open(*this);
      pixel_format = AV_PIX_FMT_RGBA;
    }
  }

  void on_data(void* p, std::size_t sz)
  {
    if(!m_running)
      return;

    if(m_rescale)
    {
      AVFrame* frame = av_frame_alloc();
      frame->format = this->pixel_format;
      frame->width = this->width;
      frame->height = this->height;

      // We are going to create a new frame in m_rescale
      // so we directly init with p.
      ::Video::initFrameFromRawData(frame, (uint8_t*)p, sz);

      ::Video::ReadFrame read{frame, 0};

      ::Video::AVFramePointer dummy;
      m_rescale.rescale(*this, m_frames, dummy, read);

      for(int i = 0; i < AV_NUM_DATA_POINTERS; ++i)
        frame->data[i] = nullptr;

      av_frame_free(&frame);

      m_frames.enqueue(read.frame);
    }
    else
    {
      ::Video::AVFramePointer frame = m_frames.newFrame();

      frame->format = this->pixel_format;
      frame->width = this->width;
      frame->height = this->height;

      // Here we need to copy the buffer.
      uint8_t* storage{};
      // Reuse allocated memory if any
      if(frame->data[0])
      {
        storage = frame->data[0];
      }
      else
      {
        // We got a new frame, init it
        auto buf = av_buffer_alloc(sz);
        storage = buf->data;
        frame->buf[0] = buf;
        ::Video::initFrameFromRawData(frame.get(), storage, sz);
      }

      // Copy the content as we're going on *adventures*
      memcpy(storage, p, sz);

      m_frames.enqueue(frame.release());
    }
  }

  ::Video::FrameQueue m_frames;
  ::Video::Rescale m_rescale;

  std::atomic_bool m_running{};

  std::string m_path;
  shmdata::ConsoleLogger m_logger;
  std::optional<shmdata::Reader> m_receiver;
};


InputDevice::~InputDevice() { }

bool InputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<InputSettings>();

    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if (plug)
    {
      auto stream = std::make_shared<InputStream>(set.path);

      m_protocol = new Gfx::video_texture_input_protocol{std::move(stream), plug->exec};
      m_dev = std::make_unique<Gfx::video_texture_input_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          this->settings().name.toStdString());
    }
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("Shmdata Input");
}

QString InputFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* InputFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface*
InputFactory::makeDevice(const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& InputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Shmdata Input";
    InputSettings specif;
    specif.path = "/tmp/score_shmdata_input";
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* InputFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* InputFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* InputFactory::makeSettingsWidget()
{
  return new InputSettingsWidget;
}

QVariant InputFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<InputSettings>(visitor);
}

void InputFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<InputSettings>(data, visitor);
}

bool InputFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

InputSettingsWidget::InputSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Shmdata path"), m_shmPath = new QLineEdit);
  setLayout(layout);

  setSettings(InputFactory{}.defaultSettings());
}


Device::DeviceSettings InputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = InputFactory::static_concreteKey();
  InputSettings set;
  set.path = m_shmPath->text();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void InputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);

  const auto& set = settings.deviceSpecificSettings.value<InputSettings>();
  m_shmPath->setText(set.path);
}

}

template <>
void DataStreamReader::read(const Gfx::Shmdata::InputSettings& n)
{
  m_stream << n.path;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Shmdata::InputSettings& n)
{
  m_stream >> n.path;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Shmdata::InputSettings& n)
{
  obj["Path"] = n.path;
}

template <>
void JSONWriter::write(Gfx::Shmdata::InputSettings& n)
{
  n.path = obj["Path"].toString();
}
