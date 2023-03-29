#include "ShmdataInputDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/FrameQueue.hpp>
#include <Video/GStreamerCompatibility.hpp>
#include <Video/GpuFormats.hpp>
#include <Video/Rescale.hpp>
#include <Video/VideoInterface.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/fmt.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>

#include <shmdata/reader.hpp>

#include <wobjectimpl.h>

#include <functional>

#include <shmdata/console-logger.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace Gfx::Shmdata
{

class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  Gfx::video_texture_input_protocol* m_protocol{};
  mutable std::unique_ptr<Gfx::video_texture_input_device> m_dev;
};

}

W_OBJECT_IMPL(Gfx::Shmdata::InputDevice)

namespace Gfx::Shmdata
{

class InputStream final : public ::Video::ExternalInput
{
public:
  explicit InputStream(const QString& path) noexcept
      : m_path{path.toStdString()}
      , m_receiver{
            std::in_place_t{},
            m_path,
            [this](void* p, size_t sz) { on_data(p, sz); },
            [this](const std::string& s) { setup(s); },
            []() {},
            &m_logger}
  {
    realTime = true;
  }

  ~InputStream() noexcept { stop(); }

  bool start() noexcept override
  {
    if(m_running)
      return false;

    if(!m_receiver)
    {
      m_receiver.emplace(
          m_path, [this](void* p, size_t sz) { on_data(p, sz); },
          [this](const std::string& s) { setup(s); }, []() {}, &m_logger);
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

  AVFrame* dequeue_frame() noexcept override { return m_frames.dequeue(); }

  void release_frame(AVFrame* frame) noexcept override { m_frames.release(frame); }

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
    for(auto& elt : split)
    {
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
          if(parts.size() == 2)
          {
            double num = parts[0].toDouble();
            double denom = parts[1].toDouble();
            if(denom > 0)
            {
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
      m_rescale.rescale(m_frames, dummy, read);

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
      const auto storage = ::Video::initFrameBuffer(*frame, sz);
      ::Video::initFrameFromRawData(frame.get(), storage, sz);

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
    auto set = this->settings().deviceSpecificSettings.value<SharedInputSettings>();

    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      auto stream = std::make_shared<InputStream>(set.path);

      m_protocol = new Gfx::video_texture_input_protocol{std::move(stream), plug->exec};
      m_dev = std::make_unique<Gfx::video_texture_input_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          this->settings().name.toStdString());
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

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("Shmdata Input");
}

Device::DeviceInterface* InputFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& InputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Shmdata Input";
    SharedInputSettings specif;
    specif.path = "/tmp/score_shmdata_input";
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* InputFactory::makeSettingsWidget()
{
  return new InputSettingsWidget;
}

InputSettingsWidget::InputSettingsWidget(QWidget* parent)
    : SharedInputSettingsWidget{parent}
{
  m_deviceNameEdit->setText("Shmdata In");
  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Shmdata path");
  setSettings(InputFactory{}.defaultSettings());
}

Device::DeviceSettings InputSettingsWidget::getSettings() const
{
  auto set = SharedInputSettingsWidget::getSettings();
  set.protocol = InputFactory::static_concreteKey();
  return set;
}

}
