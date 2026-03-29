#include "GStreamerDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Audio/Settings/Model.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/VideoNode.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/name_utils.hpp>
#include <ossia/detail/dylib_loader.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTimer>

#include <QSpinBox>
#include <wobjectimpl.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
}

#include <Gfx/GStreamer/GStreamerLoader.hpp>
#include <Video/GStreamerCompatibility.hpp>

#include <climits>
#include <regex>
#include <thread>

namespace Gfx::GStreamer
{

struct gstreamer_pipeline
{
  struct AppsinkInfo
  {
    std::string name;
    bool is_video{};
    int width{};
    int height{};
    AVPixelFormat pixfmt{AV_PIX_FMT_NONE};
    int channels{};
    int rate{};
  };

  std::vector<AppsinkInfo> appsinks;
  std::vector<std::shared_ptr<::Video::FrameQueue>> video_queues;

  // Named elements with their GObject properties exposed as ossia parameters
  std::vector<std::pair<std::string, GstElement*>> named_elements;

  // Audio: GStreamer delivers large chunks (e.g. 1024 samples).
  // The audio engine reads small chunks (e.g. 64 samples).
  // We use a lock-free ring buffer to bridge the two.
  struct AudioBuffer
  {
    int sample_rate{48000};
    int num_channels{2};

    // Ring buffer per channel, written by GStreamer thread, read by audio engine
    static constexpr std::size_t ring_size = 65536;
    std::vector<std::vector<float>> ring; // [channel][ring_size]
    std::atomic<std::size_t> write_pos{0};
    std::atomic<std::size_t> read_pos{0};

    // Backing storage for audio spans — audio engine reads from here
    std::vector<ossia::float_vector>* output_data{};

    void init(int nchannels)
    {
      num_channels = nchannels;
      ring.resize(nchannels);
      for(auto& ch : ring)
        ch.resize(ring_size, 0.f);
    }

    // Called by GStreamer thread: write deinterleaved samples into ring
    void write(const float* interleaved, int num_samples, int channels)
    {
      int nch = std::min(channels, num_channels);
      auto wp = write_pos.load(std::memory_order_relaxed);
      for(int s = 0; s < num_samples; s++)
      {
        for(int ch = 0; ch < nch; ch++)
          ring[ch][(wp + s) % ring_size] = interleaved[s * channels + ch];
      }
      write_pos.store(wp + num_samples, std::memory_order_release);
    }

    // Called by audio engine (indirectly): copy from ring into output spans
    void read_into_output(int block_size)
    {
      if(!output_data)
        return;
      auto rp = read_pos.load(std::memory_order_relaxed);
      auto wp = write_pos.load(std::memory_order_acquire);

      // How many samples are available?
      std::size_t available = (wp >= rp) ? (wp - rp) : 0;

      int nch = std::min((int)output_data->size(), num_channels);
      if(available >= (std::size_t)block_size)
      {
        // Copy block_size samples from ring to output
        for(int ch = 0; ch < nch; ch++)
        {
          auto& dst = (*output_data)[ch];
          auto& src = ring[ch];
          for(int s = 0; s < block_size; s++)
            dst[s] = src[(rp + s) % ring_size];
        }
        read_pos.store(rp + block_size, std::memory_order_release);
      }
      else
      {
        // Underrun: output silence
        for(int ch = 0; ch < nch; ch++)
          std::fill_n((*output_data)[ch].data(), block_size, 0.f);
      }
    }
  };
  std::vector<std::unique_ptr<AudioBuffer>> audio_buffers;

  gstreamer_pipeline() = default;
  ~gstreamer_pipeline() { cleanup(); }

  bool load(const std::string& pipeline_string)
  {
    auto& gst = libgstreamer::instance();
    if(!gst.available)
      return false;

    if(!gstreamer_init())
      return false;

    // Parse the pipeline
    GError* err = nullptr;
    m_pipeline = gst.parse_launch(pipeline_string.c_str(), &err);
    if(err)
    {
      qDebug() << "GStreamer parse error:" << err->message;
      if(gst.g_error_free)
        gst.g_error_free(err);
      return false;
    }
    if(!m_pipeline)
      return false;

    // Find appsink names from the pipeline string via regex
    auto sink_names = find_appsink_names(pipeline_string);
    if(sink_names.empty())
    {
      qDebug() << "GStreamer: no appsink elements found in pipeline";
      gst.object_unref(m_pipeline);
      m_pipeline = nullptr;
      return false;
    }

    // Set to PAUSED to negotiate caps
    gst.element_set_state(m_pipeline, GST_STATE_PAUSED);

    GstState state{};
    auto ret = gst.element_get_state(
        m_pipeline, &state, nullptr, 5 * GST_SECOND);
    if(ret == GST_STATE_CHANGE_FAILURE)
    {
      qDebug() << "GStreamer: failed to set pipeline to PAUSED";
      gst.element_set_state(m_pipeline, GST_STATE_NULL);
      gst.object_unref(m_pipeline);
      m_pipeline = nullptr;
      return false;
    }

    // Discover appsink types
    int video_count = 0;
    int audio_count = 0;
    for(auto& sink_name : sink_names)
    {
      GstElement* element = gst.bin_get_by_name(m_pipeline, sink_name.c_str());
      if(!element)
      {
        qDebug() << "GStreamer: appsink not found:" << sink_name.c_str();
        continue;
      }

      m_appsink_elements.push_back(element);

      AppsinkInfo info;
      info.name = sink_name;

      // Get the sink pad and query caps
      if(gst.element_get_static_pad && gst.pad_get_current_caps
         && gst.caps_get_structure && gst.structure_get_name)
      {
        GstPad* pad = gst.element_get_static_pad(element, "sink");
        if(pad)
        {
          GstCaps* caps = gst.pad_get_current_caps(pad);
          if(caps)
          {
            GstStructure* s = gst.caps_get_structure(caps, 0);
            if(s)
            {
              const char* media_type = gst.structure_get_name(s);
              if(media_type)
              {
                std::string type_str(media_type);
                if(type_str.find("video/") == 0)
                {
                  info.is_video = true;
                  parse_video_caps(gst, s, info);
                  video_count++;
                }
                else if(type_str.find("audio/") == 0)
                {
                  info.is_video = false;
                  parse_audio_caps(gst, s, info);
                  audio_count++;
                }
              }
            }
            if(gst.caps_unref)
              gst.caps_unref(caps);
          }
          else
          {
            // Caps not yet negotiated; default to video
            info.is_video = true;
            info.width = 640;
            info.height = 480;
            info.pixfmt = AV_PIX_FMT_RGBA;
            video_count++;
          }
          gst.object_unref(pad);
        }
      }

      appsinks.push_back(info);
    }

    // Create frame queues and audio buffers
    for(auto& sink : appsinks)
    {
      if(sink.is_video)
      {
        video_queues.push_back(std::make_shared<::Video::FrameQueue>());
      }
      else
      {
        auto buf = std::make_unique<AudioBuffer>();
        buf->sample_rate = sink.rate;
        buf->init(sink.channels);
        audio_buffers.push_back(std::move(buf));
      }
    }

    // Discover named elements (excluding appsink/appsrc) for property exposure
    auto all_names = find_all_named_elements(pipeline_string);
    for(auto& ename : all_names)
    {
      // Skip appsink/appsrc names — already handled
      bool is_appsink_or_src = false;
      for(auto& s : appsinks)
        if(s.name == ename)
          is_appsink_or_src = true;
      if(is_appsink_or_src)
        continue;

      GstElement* elem = gst.bin_get_by_name(m_pipeline, ename.c_str());
      if(elem)
        named_elements.emplace_back(ename, elem);
    }

    // Set back to NULL, ready for start()
    gst.element_set_state(m_pipeline, GST_STATE_NULL);
    return true;
  }

  void start()
  {
    if(!m_pipeline)
    {
      return;
    }

    auto& gst = libgstreamer::instance();
    gst.element_set_state(m_pipeline, GST_STATE_PLAYING);

    if(gst.element_get_state)
    {
      GstState state{};
      gst.element_get_state(m_pipeline, &state, nullptr, 5 * GST_SECOND);
    }

    m_running = true;
    m_thread = std::thread([this] { pipeline_loop(); });
  }

  void stop()
  {
    m_running = false;
    if(m_thread.joinable())
      m_thread.join();

    if(m_pipeline)
    {
      auto& gst = libgstreamer::instance();
      gst.element_set_state(m_pipeline, GST_STATE_NULL);
    }

    for(auto& q : video_queues)
      q->drain();
  }

  void cleanup()
  {
    stop();

    if(m_pipeline)
    {
      auto& gst = libgstreamer::instance();

      for(auto* elem : m_appsink_elements)
        gst.object_unref(elem);
      m_appsink_elements.clear();

      for(auto& [name, elem] : named_elements)
        gst.object_unref(elem);
      named_elements.clear();

      gst.object_unref(m_pipeline);
      m_pipeline = nullptr;
    }
  }

private:
  static std::vector<std::string> find_appsink_names(const std::string& pipeline)
  {
    std::vector<std::string> names;
    // Match "appsink" optionally followed by properties including name=<identifier>
    std::regex re(R"(appsink\b[^!]*?\bname\s*=\s*(\w+))");
    auto begin = std::sregex_iterator(pipeline.begin(), pipeline.end(), re);
    auto end = std::sregex_iterator();
    for(auto it = begin; it != end; ++it)
    {
      names.push_back((*it)[1].str());
    }
    return names;
  }

  // Find all name=<identifier> patterns in the pipeline string
  // (covers any element, not just appsink/appsrc)
  static std::vector<std::string> find_all_named_elements(const std::string& pipeline)
  {
    std::vector<std::string> names;
    std::regex re(R"(\bname\s*=\s*(\w+))");
    auto begin = std::sregex_iterator(pipeline.begin(), pipeline.end(), re);
    auto end = std::sregex_iterator();
    for(auto it = begin; it != end; ++it)
    {
      names.push_back((*it)[1].str());
    }
    return names;
  }

  static void parse_video_caps(
      const libgstreamer& gst, GstStructure* s, AppsinkInfo& info)
  {
    if(gst.structure_get_int)
    {
      gst.structure_get_int(s, "width", &info.width);
      gst.structure_get_int(s, "height", &info.height);
    }
    if(gst.structure_get_string)
    {
      const char* format = gst.structure_get_string(s, "format");
      if(format)
      {
        auto& map = ::Video::gstreamerToLibav();
        auto it = map.find(std::string(format));
        if(it != map.end())
          info.pixfmt = it->second;
        else
          info.pixfmt = AV_PIX_FMT_RGBA;
      }
    }
    if(info.width <= 0) info.width = 640;
    if(info.height <= 0) info.height = 480;
    if(info.pixfmt == AV_PIX_FMT_NONE) info.pixfmt = AV_PIX_FMT_RGBA;
  }

  static void parse_audio_caps(
      const libgstreamer& gst, GstStructure* s, AppsinkInfo& info)
  {
    if(gst.structure_get_int)
    {
      gst.structure_get_int(s, "channels", &info.channels);
      gst.structure_get_int(s, "rate", &info.rate);
    }
    if(info.channels <= 0) info.channels = 2;
    if(info.rate <= 0) info.rate = 48000;
  }

  void pipeline_loop()
  {
    auto& gst = libgstreamer::instance();

    while(m_running)
    {
      bool got_sample = false;
      int video_idx = 0;
      int audio_idx = 0;

      for(std::size_t i = 0; i < appsinks.size(); i++)
      {
        auto* element = m_appsink_elements[i];
        auto& info = appsinks[i];

        // Pull sample with 10ms timeout
        GstSample* sample = gst.app_sink_try_pull_sample(
            element, 10000000 /* 10ms in ns */);
        if(!sample)
        {
          if(info.is_video)
            video_idx++;
          else
            audio_idx++;
          continue;
        }

        got_sample = true;
        GstBuffer* buffer = gst.sample_get_buffer(sample);
        if(!buffer)
        {
          gst.mini_object_unref(sample);
          if(info.is_video) video_idx++;
          else audio_idx++;
          continue;
        }

        GstMapInfo map_info{};
        if(gst.buffer_map(buffer, &map_info, GST_MAP_READ))
        {
          if(info.is_video)
          {
            process_video_frame(map_info, info, video_idx);
          }
          else
          {
            process_audio_frame(map_info, info, audio_idx);
          }
          gst.buffer_unmap(buffer, &map_info);
        }

        gst.mini_object_unref(sample);

        if(info.is_video) video_idx++;
        else audio_idx++;
      }

      if(!got_sample)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  void process_video_frame(
      const GstMapInfo& map_info, const AppsinkInfo& info, int video_idx)
  {
    if(video_idx < 0 || video_idx >= (int)video_queues.size())
      return;

    auto& queue = *video_queues[video_idx];
    auto frame = queue.newFrame();
    if(!frame)
      return;

    frame->format = info.pixfmt;
    frame->width = info.width;
    frame->height = info.height;

    // Allocate storage for the frame data
    auto* storage = ::Video::initFrameBuffer(*frame, map_info.size);
    if(!storage || !map_info.data)
      return;

    // Set up the plane pointers (data[] and linesize[]) based on pixel format
    if(!::Video::initFrameFromRawData(frame.get(), storage, map_info.size))
      return;

    // Copy the actual pixel data
    memcpy(storage, map_info.data, map_info.size);

    queue.enqueue(frame.release());
  }

  void process_audio_frame(
      const GstMapInfo& map_info, const AppsinkInfo& info, int audio_idx)
  {
    if(audio_idx < 0 || audio_idx >= (int)audio_buffers.size())
      return;

    auto& buf = *audio_buffers[audio_idx];

    // Assume F32LE interleaved audio (user should use audioconvert in pipeline)
    const float* src = reinterpret_cast<const float*>(map_info.data);
    int num_samples = map_info.size / (sizeof(float) * info.channels);

    // Write into ring buffer (deinterleaves internally)
    buf.write(src, num_samples, info.channels);
  }

  GstElement* m_pipeline{};
  std::vector<GstElement*> m_appsink_elements;
  std::thread m_thread;
  std::atomic_bool m_running{};
};

class gstreamer_video_decoder : public ::Video::ExternalInput
{
  std::shared_ptr<::Video::FrameQueue> queue;

public:
  gstreamer_video_decoder(std::shared_ptr<::Video::FrameQueue> queue)
      : queue{std::move(queue)}
  {
    this->realTime = true;
    this->dts_per_flicks = 0;
    this->flicks_per_dts = 0;
  }

  ~gstreamer_video_decoder() { queue->drain(); }

  bool start() noexcept override { return true; }
  void stop() noexcept override { }

  AVFrame* dequeue_frame() noexcept override { return queue->dequeue(); }
  void release_frame(AVFrame* frame) noexcept override { queue->release(frame); }
};

class gstreamer_audio_parameter final : public ossia::audio_parameter
{
public:
  gstreamer_audio_parameter(
      gstreamer_pipeline::AudioBuffer& buf, int num_channels, int bs,
      ossia::net::node_base& n)
      : audio_parameter{n}
      , m_buffer{buf}
      , m_audio_data(num_channels)
      , m_block_size{bs}
  {
    // Set up owned buffers and point audio spans to them permanently
    audio.resize(num_channels);
    for(int i = 0; i < num_channels; i++)
    {
      m_audio_data[i].resize(bs, 0.f);
      audio[i] = m_audio_data[i];
    }
    // Give the AudioBuffer a pointer so read_into_output can fill our buffers
    m_buffer.output_data = &m_audio_data;
  }

  virtual ~gstreamer_audio_parameter() { m_buffer.output_data = nullptr; }

  // clone_value() (not virtual) reads from audio spans.
  // We use push_value(audio_port) as a hook — it's called by the audio engine
  // when data flows INTO this parameter. But for a source, we need data to
  // flow OUT. So we hook into pull_value instead, even though it may not be
  // called for non-audio-protocol params.
  //
  // Since neither hook reliably fires, we refresh data in pull_value AND
  // we also refresh it from clone_value via a const_cast trick on a mutable member.
  void pull_value() override
  {
    refresh_from_ring();
  }

  // clone_value reads from audio spans. We override it... wait, it's not virtual.
  // Instead, we make the audio spans point to buffers that are refreshed
  // by the GStreamer protocol's tick.

  void refresh_from_ring()
  {
    m_buffer.read_into_output(m_block_size);
    // Re-point spans (in case vectors reallocated, though they shouldn't)
    for(std::size_t i = 0; i < m_audio_data.size(); i++)
      audio[i] = m_audio_data[i];
  }

private:
  gstreamer_pipeline::AudioBuffer& m_buffer;
  std::vector<ossia::float_vector> m_audio_data;
  int m_block_size{};
};

class gstreamer_device;
class gstreamer_protocol : public ossia::net::protocol_base
{
public:
  gstreamer_pipeline pipeline;
  gstreamer_device* device{};

  gstreamer_protocol(const std::string& pipeline_string)
      : ossia::net::protocol_base{flags{}}
  {
    pipeline.load(pipeline_string);
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override { pipeline.start(); }
  void stop_execution() override { pipeline.stop(); }

  // Called at the beginning of each audio tick, before the graph executes.
  // This is where we copy audio from the ring buffer into the output spans.
  void pre_tick(std::size_t buffer_size) override
  {
    for(auto& buf : pipeline.audio_buffers)
    {
      buf->read_into_output(buffer_size);
    }
  }
};

class gstreamer_device : public ossia::net::device_base
{
  ossia::net::generic_node root;

public:
  gstreamer_device(
      GfxExecutionAction& ctx, std::unique_ptr<gstreamer_protocol> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{name, *this}
  {
    m_capabilities.change_tree = true;
    auto* gst_proto = static_cast<gstreamer_protocol*>(m_protocol.get());
    gst_proto->device = this;

    auto& pipe = gst_proto->pipeline;

    int video_idx = 0;
    int audio_idx = 0;
    for(auto& sink : pipe.appsinks)
    {
      if(sink.is_video)
      {
        auto decoder = std::make_shared<gstreamer_video_decoder>(
            pipe.video_queues[video_idx]);
        decoder->pixel_format = sink.pixfmt;
        decoder->width = sink.width;
        decoder->height = sink.height;
        decoder->fps = 30;

        root.add_child(std::make_unique<Gfx::simple_texture_input_node>(
            new score::gfx::CameraNode(decoder), &ctx, *this, sink.name));

        video_idx++;
      }
      else
      {
        auto& audio_stgs
            = score::AppContext().settings<Audio::Settings::Model>();
        auto audio_node = std::make_unique<ossia::net::generic_node>(
            sink.name, *this, root);
        audio_node->set_parameter(std::make_unique<gstreamer_audio_parameter>(
            *pipe.audio_buffers[audio_idx], sink.channels,
            audio_stgs.getBufferSize(), *audio_node));
        root.add_child(std::move(audio_node));

        audio_idx++;
      }
    }

    // Expose GObject properties of named elements as ossia parameters
    expose_element_properties(pipe);
  }

  void expose_element_properties(gstreamer_pipeline& pipe)
  {
    auto& gst = libgstreamer::instance();
    if(!gst.object_class_list_properties || !gst.type_class_ref
       || !gst.object_set_property || !gst.value_init || !gst.value_unset)
      return;

    for(auto& [elem_name, element] : pipe.named_elements)
    {
      // Create a sub-node for this element
      auto elem_node = std::make_unique<ossia::net::generic_node>(
          elem_name, *this, root);

      // Get the GObject class to enumerate properties
      // G_OBJECT_GET_CLASS(obj) = ((GTypeInstance*)obj)->g_class
      // but we can use g_type_class_ref with the element's GType
      // The element itself IS a GObject, so we can cast and get properties
      // GObjectClass* klass = G_OBJECT_GET_CLASS(element);
      // We access it via the first pointer in the struct (GTypeInstance.g_class)
      void** type_instance = reinterpret_cast<void**>(element);
      void* g_class = type_instance[0]; // First field of GTypeInstance

      unsigned int n_props = 0;
      GParamSpec** props
          = gst.object_class_list_properties(g_class, &n_props);

      for(unsigned int i = 0; i < n_props; i++)
      {
        GParamSpec* pspec = props[i];
        if(!pspec || !pspec->name)
          continue;

        // Only expose writable properties
        if(!(pspec->flags & G_PARAM_WRITABLE))
          continue;

        // Skip "name" and "parent" — internal GStreamer properties
        std::string prop_name(pspec->name);
        if(prop_name == "name" || prop_name == "parent")
          continue;

        auto prop_node = std::make_unique<ossia::net::generic_node>(
            prop_name, *this, *elem_node);

        ossia::net::parameter_base* param = nullptr;
        GType vtype = pspec->value_type;

        if(vtype == G_TYPE_INT)
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecInt*>(pspec);
            param->set_domain(
                ossia::make_domain(ps->minimum, ps->maximum));
            param->push_value(ps->default_value);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_INT);
                    gst.value_set_int(&gv, *val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_UINT)
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecUInt*>(pspec);
            int mn = (int)std::min(ps->minimum, (unsigned int)INT_MAX);
            int mx = (int)std::min(ps->maximum, (unsigned int)INT_MAX);
            param->set_domain(ossia::make_domain(mn, mx));
            param->push_value((int)std::min(ps->default_value, (unsigned int)INT_MAX));
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_UINT);
                    gst.value_set_uint(&gv, (unsigned int)*val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_INT64)
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecInt64*>(pspec);
            param->push_value((int)ps->default_value);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_INT64);
                    gst.value_set_int64(&gv, (int64_t)*val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_UINT64)
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecUInt64*>(pspec);
            param->push_value((int)ps->default_value);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_UINT64);
                    gst.value_set_uint64(&gv, (uint64_t)*val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_FLOAT)
        {
          param = prop_node->create_parameter(ossia::val_type::FLOAT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecFloat*>(pspec);
            param->set_domain(
                ossia::make_domain(ps->minimum, ps->maximum));
            param->push_value(ps->default_value);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<float>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_FLOAT);
                    gst.value_set_float(&gv, *val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_DOUBLE)
        {
          param = prop_node->create_parameter(ossia::val_type::FLOAT);
          if(param)
          {
            auto* ps = reinterpret_cast<GParamSpecDouble*>(pspec);
            param->set_domain(ossia::make_domain(
                (float)ps->minimum, (float)ps->maximum));
            param->push_value((float)ps->default_value);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<float>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_DOUBLE);
                    gst.value_set_double(&gv, (double)*val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_BOOLEAN)
        {
          param = prop_node->create_parameter(ossia::val_type::BOOL);
          if(param)
          {
            param->push_value(false);
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<bool>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_BOOLEAN);
                    gst.value_set_boolean(&gv, *val ? 1 : 0);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(vtype == G_TYPE_STRING)
        {
          param = prop_node->create_parameter(ossia::val_type::STRING);
          if(param)
          {
            param->push_value(std::string{});
            param->add_callback(
                [&gst, element, pn = prop_name](const ossia::value& v) {
                  if(auto val = v.target<std::string>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, G_TYPE_STRING);
                    gst.value_set_string(&gv, val->c_str());
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(gst.type_is_a && gst.type_is_a(vtype, G_TYPE_ENUM))
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            param->push_value(0);
            // Use the actual derived GType, not G_TYPE_ENUM
            param->add_callback(
                [&gst, element, pn = prop_name, vtype](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, vtype);
                    gst.value_set_enum(&gv, *val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        else if(gst.type_is_a && gst.type_is_a(vtype, G_TYPE_FLAGS))
        {
          param = prop_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            param->push_value(0);
            param->add_callback(
                [&gst, element, pn = prop_name, vtype](const ossia::value& v) {
                  if(auto val = v.target<int>())
                  {
                    GValue gv{};
                    gst.value_init(&gv, vtype);
                    // g_value_set_flags has the same signature as set_enum
                    gst.value_set_enum(&gv, *val);
                    gst.object_set_property(element, pn.c_str(), &gv);
                    gst.value_unset(&gv);
                  }
                });
          }
        }
        // Skip other types (OBJECT, POINTER, BOXED, etc.)

        if(param)
        {
          param->set_access(ossia::access_mode::BI);

          // Set description from GParamSpec blurb
          if(pspec->_blurb && pspec->_blurb[0])
            ossia::net::set_description(*prop_node, pspec->_blurb);

          // Set default value
          ossia::net::set_default_value(*prop_node, param->value());

          elem_node->add_child(std::move(prop_node));
        }
      }

      if(gst.g_free)
        gst.g_free(props);

      root.add_child(std::move(elem_node));
    }
  }

  const ossia::net::generic_node& get_root_node() const override { return root; }
  ossia::net::generic_node& get_root_node() override { return root; }
};

// Score-facing device
class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gstreamer_protocol* m_protocol{};
  mutable std::unique_ptr<gstreamer_device> m_dev;
};

W_OBJECT_IMPL(Gfx::GStreamer::InputDevice)

InputDevice::~InputDevice() { }

bool InputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<GStreamerSettings>();
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      auto proto = std::make_unique<gstreamer_protocol>(
          set.pipeline.toStdString());
      m_protocol = proto.get();
      m_dev = std::make_unique<gstreamer_device>(
          plug->exec, std::move(proto), this->settings().name.toStdString());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "GStreamer: Could not connect:" << e.what();
  }
  catch(...)
  {
  }

  return connected();
}

// Settings widget
class GStreamerSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit GStreamerSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QPlainTextEdit* m_pipeline{};
  QSpinBox* m_width{};
  QSpinBox* m_height{};
  QSpinBox* m_rate{};
  QSpinBox* m_audioChannels{};
  QComboBox* m_inputTransfer{};
  QLabel* m_outputLabel{};
  QLabel* m_validationLabel{};
  Device::DeviceSettings m_settings;

  void validatePipeline();
};

GStreamerSettingsWidget::GStreamerSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  m_pipeline = new QPlainTextEdit{this};
  m_pipeline->setPlaceholderText(
      "Input:  videotestsrc name=src ! videoconvert "
      "! video/x-raw,format=RGBA ! appsink name=video\n"
      "Output: appsrc name=video ! videoconvert "
      "! autovideosink");

  m_outputLabel = new QLabel{tr("Output settings (for appsrc pipelines):"), this};
  m_width = new QSpinBox{this};
  m_width->setRange(1, 7680);
  m_width->setValue(1280);
  m_height = new QSpinBox{this};
  m_height->setRange(1, 4320);
  m_height->setValue(720);
  m_rate = new QSpinBox{this};
  m_rate->setRange(1, 240);
  m_rate->setValue(30);
  m_audioChannels = new QSpinBox{this};
  m_audioChannels->setRange(0, 8);
  m_audioChannels->setValue(2);

  m_inputTransfer = new QComboBox{this};
  // Items store AVColorTransferCharacteristic values as user data.
  m_inputTransfer->addItem(tr("sRGB (default)"), 13);  // AVCOL_TRC_IEC61966_2_1
  m_inputTransfer->addItem(tr("Linear"), 8);            // AVCOL_TRC_LINEAR
  m_inputTransfer->addItem(tr("HDR10 (PQ)"), 16);       // AVCOL_TRC_SMPTE2084
  m_inputTransfer->addItem(tr("HLG"), 18);               // AVCOL_TRC_ARIB_STD_B67
  m_inputTransfer->addItem(tr("Passthrough"), 2);        // AVCOL_TRC_UNSPECIFIED

  m_validationLabel = new QLabel{this};
  m_validationLabel->setWordWrap(true);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Pipeline"), m_pipeline);
  layout->addRow(m_validationLabel);
  layout->addRow(m_outputLabel);
  layout->addRow(tr("Width"), m_width);
  layout->addRow(tr("Height"), m_height);
  layout->addRow(tr("Rate"), m_rate);
  layout->addRow(tr("Audio Channels"), m_audioChannels);
  layout->addRow(tr("Input Transfer"), m_inputTransfer);
  setLayout(layout);

  m_deviceNameEdit->setText("GStreamer");

  // Validate pipeline when text changes (with a short delay to avoid
  // validating on every keystroke)
  auto* validateTimer = new QTimer{this};
  validateTimer->setSingleShot(true);
  validateTimer->setInterval(500);
  connect(m_pipeline, &QPlainTextEdit::textChanged, validateTimer,
          qOverload<>(&QTimer::start));
  connect(validateTimer, &QTimer::timeout, this,
          &GStreamerSettingsWidget::validatePipeline);
}

void GStreamerSettingsWidget::validatePipeline()
{
  auto text = m_pipeline->toPlainText().trimmed();
  if(text.isEmpty())
  {
    m_validationLabel->clear();
    return;
  }

  auto& gst = libgstreamer::instance();
  if(!gst.available || !gstreamer_init())
  {
    m_validationLabel->setText(
        tr("<span style='color: orange;'>GStreamer not available</span>"));
    return;
  }

  GError* err = nullptr;
  GstElement* pipeline = gst.parse_launch(text.toStdString().c_str(), &err);

  if(err)
  {
    QString msg = QString::fromUtf8(err->message);
    m_validationLabel->setText(
        tr("<span style='color: red;'>%1</span>").arg(msg.toHtmlEscaped()));
    if(gst.g_error_free)
      gst.g_error_free(err);
  }
  else
  {
    m_validationLabel->setText(
        tr("<span style='color: green;'>Pipeline valid</span>"));
  }

  if(pipeline)
    gst.object_unref(pipeline);
}

Device::DeviceSettings GStreamerSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = ProtocolFactory::static_concreteKey();

  GStreamerSettings gset;
  gset.pipeline = m_pipeline->toPlainText();
  gset.width = m_width->value();
  gset.height = m_height->value();
  gset.rate = m_rate->value();
  gset.audio_channels = m_audioChannels->value();
  gset.input_transfer = m_inputTransfer->currentData().toInt();
  s.deviceSpecificSettings = QVariant::fromValue(gset);
  return s;
}

void GStreamerSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.trimmed();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_deviceNameEdit->setText(prettyName);

  if(settings.deviceSpecificSettings.canConvert<GStreamerSettings>())
  {
    const auto& gset
        = settings.deviceSpecificSettings.value<GStreamerSettings>();
    m_pipeline->setPlainText(gset.pipeline);
    m_width->setValue(gset.width);
    m_height->setValue(gset.height);
    m_rate->setValue(gset.rate);
    m_audioChannels->setValue(gset.audio_channels);
    int idx = m_inputTransfer->findData(gset.input_transfer);
    if(idx >= 0)
      m_inputTransfer->setCurrentIndex(idx);
  }
}

// ProtocolFactory implementation
QString ProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("GStreamer");
}

QString ProtocolFactory::category() const noexcept
{
  return StandardCategories::media;
}

QUrl ProtocolFactory::manual() const noexcept
{
  return {};
}

namespace
{
struct GStreamerPresetEnumerator : public Device::DeviceEnumerator
{
  struct Preset
  {
    QString label;
    QString name;
    QString pipeline;
    int width{1280};
    int height{720};
    int rate{30};
    int audio_channels{0};
  };

  std::vector<Preset> presets;

  void enumerate(
      std::function<void(const QString&, const Device::DeviceSettings&)> f) const override
  {
    for(auto& p : presets)
    {
      Device::DeviceSettings s;
      s.name = p.name;
      s.protocol = ProtocolFactory::static_concreteKey();
      GStreamerSettings gset;
      gset.pipeline = p.pipeline;
      gset.width = p.width;
      gset.height = p.height;
      gset.rate = p.rate;
      gset.audio_channels = p.audio_channels;
      s.deviceSpecificSettings = QVariant::fromValue(gset);
      f(p.label, s);
    }
  }
};
}

Device::DeviceEnumerators
ProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  Device::DeviceEnumerators enums;

  // ── Stream In ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"UDP MJPEG receiver",
         "GStreamer In",
         "udpsrc port=5000 "
         "caps=\"application/x-rtp,media=video,encoding-name=JPEG,payload=26,"
         "clock-rate=90000\" ! rtpjpegdepay ! jpegdec ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video sync=false"});
    e->presets.push_back(
        {"UDP H.264 receiver",
         "GStreamer In",
         "udpsrc port=5000 "
         "caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96,"
         "clock-rate=90000\" ! rtph264depay ! avdec_h264 ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video sync=false"});
    e->presets.push_back(
        {"RTSP stream",
         "GStreamer In",
         "rtspsrc location=rtsp://192.168.1.100:554/stream latency=200 ! "
         "decodebin ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video"});
    e->presets.push_back(
        {"SRT receiver",
         "GStreamer In",
         "srtsrc uri=srt://127.0.0.1:4200?mode=caller ! tsdemux ! h264parse ! "
         "avdec_h264 ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video"});
    e->presets.push_back(
        {"RIST receiver",
         "GStreamer In",
         "ristsrc address=0.0.0.0 port=5004 ! rtph264depay ! avdec_h264 ! "
         "videoconvert ! video/x-raw,format=RGBA ! appsink name=video sync=false"});
    e->presets.push_back(
        {"WHEP receiver (WebRTC)",
         "GStreamer In",
         "whepsrc whep-endpoint=http://localhost:8080/whep ! "
         "decodebin ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video"});
    enums.push_back({"Stream In", e});
  }

  // ── File In ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"Video file",
         "GStreamer In",
         "filesrc location=/path/to/video.mp4 ! decodebin ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video"});
    e->presets.push_back(
        {"Video file + audio",
         "GStreamer In",
         "filesrc location=/path/to/video.mp4 ! decodebin name=demux "
         "demux. ! queue ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video "
         "demux. ! queue ! audioconvert ! audio/x-raw,format=F32LE,rate=48000,channels=2 ! "
         "appsink name=audio"});
    enums.push_back({"File In", e});
  }

  // ── Device In ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"Test pattern",
         "GStreamer In",
         "videotestsrc ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video"});
    e->presets.push_back(
        {"Test pattern + tone",
         "GStreamer In",
         "videotestsrc ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video "
         "audiotestsrc wave=sine freq=440 ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
    // Cross-platform devices
    e->presets.push_back(
        {"JACK audio input",
         "GStreamer In",
         "jackaudiosrc ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
    e->presets.push_back(
        {"Decklink video input",
         "GStreamer In",
         "decklinkvideosrc mode=auto ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video"});

#if defined(__linux__)
    e->presets.push_back(
        {"Screen capture (X11)",
         "GStreamer In",
         "ximagesrc ! videoconvert ! video/x-raw,format=RGBA ! appsink name=video"});
    e->presets.push_back(
        {"Webcam (V4L2)",
         "GStreamer In",
         "v4l2src device=/dev/video0 ! videoconvert ! video/x-raw,format=RGBA ! "
         "appsink name=video"});
    e->presets.push_back(
        {"Microphone (PipeWire)",
         "GStreamer In",
         "pipewiresrc ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
    e->presets.push_back(
        {"ALSA audio input",
         "GStreamer In",
         "alsasrc ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
#elif defined(_WIN32)
    e->presets.push_back(
        {"WASAPI audio input",
         "GStreamer In",
         "wasapi2src ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
#elif defined(__APPLE__)
    e->presets.push_back(
        {"CoreAudio input",
         "GStreamer In",
         "osxaudiosrc ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! appsink name=audio"});
#endif
    enums.push_back({"Device In", e});
  }

  // ── Stream Out ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"UDP MJPEG streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! video/x-raw,format=I420 ! "
         "jpegenc ! rtpjpegpay ! udpsink host=127.0.0.1 port=5000 sync=false"});
    e->presets.push_back(
        {"UDP H.264 streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency "
         "bitrate=2000 key-int-max=30 ! rtph264pay ! "
         "udpsink host=127.0.0.1 port=5000 sync=false"});
    e->presets.push_back(
        {"SRT H.264 streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! "
         "mpegtsmux ! srtsink uri=srt://:4200?mode=listener"});
    e->presets.push_back(
        {"RIST H.264 streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! "
         "rtph264pay ! ristsink address=127.0.0.1 port=5004"});
    e->presets.push_back(
        {"WHIP sender (WebRTC)",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! "
         "rtph264pay ! whipsink whip-endpoint=http://localhost:8080/whip"});
    e->presets.push_back(
        {"RTP multicast (SMPTE 2022-1 FEC)",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! "
         "rtph264pay ! rtpst2022-1-fecenc ! "
         "udpsink host=239.0.0.1 port=5004 auto-multicast=true sync=false"});
    e->presets.push_back(
        {"TCP JPEG streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! jpegenc ! "
         "tcpserversink host=127.0.0.1 port=5000"});
    enums.push_back({"Stream Out", e});
  }

  // ── Record Video ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"Record MP4 H.264",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency "
         "bitrate=4000 ! mp4mux ! filesink location=/tmp/output.mp4"});
    e->presets.push_back(
        {"Record MP4 H.264 + AAC",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! mux. "
         "appsrc name=audio ! audioconvert ! voaacenc ! mux. "
         "mp4mux name=mux ! filesink location=/tmp/output.mp4",
         1280, 720, 30, 2});
    e->presets.push_back(
        {"Record MKV H.264",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency "
         "bitrate=4000 ! matroskamux ! filesink location=/tmp/output.mkv"});
    enums.push_back({"Record Video", e});
  }

  // ── Device Out ──
  {
    auto* e = new GStreamerPresetEnumerator;

    // Video display (cross-platform)
    e->presets.push_back(
        {"Display window",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! autovideosink"});

    // Blackmagic Decklink (cross-platform — available on Linux, macOS, Windows)
    e->presets.push_back(
        {"Decklink video output",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! video/x-raw,format=UYVY ! "
         "decklinkvideosink mode=auto"});
    e->presets.push_back(
        {"Decklink video + audio output",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! video/x-raw,format=UYVY ! "
         "decklinkvideosink mode=auto "
         "appsrc name=audio ! audioconvert ! decklinkaudiosink",
         1920, 1080, 30, 2});

    // JACK audio output (cross-platform)
    e->presets.push_back(
        {"JACK audio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! jackaudiosink",
         1280, 720, 30, 2});

#if defined(_WIN32)
    // Windows audio output
    e->presets.push_back(
        {"WASAPI audio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! wasapi2sink",
         1280, 720, 30, 2});
#elif defined(__linux__)
    // V4L2 virtual webcam (Linux only)
    e->presets.push_back(
        {"Virtual webcam (V4L2 loopback)",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! video/x-raw,format=YUY2 ! "
         "v4l2sink device=/dev/video10"});

    // Linux audio outputs
    e->presets.push_back(
        {"PulseAudio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! pulsesink",
         1280, 720, 30, 2});
    e->presets.push_back(
        {"PipeWire audio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! pipewiresink",
         1280, 720, 30, 2});
    e->presets.push_back(
        {"ALSA audio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! alsasink",
         1280, 720, 30, 2});

    // Shared memory IPC (Linux/POSIX)
    e->presets.push_back(
        {"Shared memory video output",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! shmsink socket-path=/tmp/score_video "
         "shm-size=20000000 wait-for-connection=false"});
#elif defined(__APPLE__)
    // macOS audio output
    e->presets.push_back(
        {"CoreAudio output",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! osxaudiosink",
         1280, 720, 30, 2});
#endif

    enums.push_back({"Device Out", e});
  }

  // ── HW Accelerated ──
  {
    auto* e = new GStreamerPresetEnumerator;
    e->presets.push_back(
        {"NVENC H.264 to MP4",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! nvh264enc preset=low-latency-hq ! "
         "h264parse ! mp4mux ! filesink location=/tmp/output_nvenc.mp4"});
    e->presets.push_back(
        {"NVENC H.265 to MKV",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! nvh265enc preset=low-latency-hq ! "
         "h265parse ! matroskamux ! filesink location=/tmp/output_nvenc.mkv"});
    e->presets.push_back(
        {"QSV H.264 to MP4",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! qsvh264enc ! "
         "h264parse ! mp4mux ! filesink location=/tmp/output_qsv.mp4"});
    e->presets.push_back(
        {"NVENC H.264 UDP streaming",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! nvh264enc preset=low-latency-hq ! "
         "rtph264pay ! udpsink host=127.0.0.1 port=5000 sync=false"});
    enums.push_back({"HW Accelerated", e});
  }

  // ── Broadcast / SMPTE 2110 ──
  {
    auto* e = new GStreamerPresetEnumerator;

    // ST 2110-20: Uncompressed video over RTP (raw)
    e->presets.push_back(
        {"ST 2110-20 video sender (raw)",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! "
         "video/x-raw,format=UYVP,width=1920,height=1080,framerate=30/1 ! "
         "rtpvrawpay ! udpsink host=239.0.0.1 port=5004 "
         "auto-multicast=true sync=false"});
    e->presets.push_back(
        {"ST 2110-20 video receiver (raw)",
         "GStreamer In",
         "udpsrc address=239.0.0.1 port=5004 multicast-group=239.0.0.1 "
         "caps=\"application/x-rtp,media=video,encoding-name=RAW,"
         "sampling=YCbCr-4:2:2,depth=(string)10,width=(string)1920,"
         "height=(string)1080,clock-rate=90000\" ! "
         "rtpvrawdepay ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video sync=false"});

    // ST 2110-30 / AES 67: PCM audio over RTP (L16/L24)
    e->presets.push_back(
        {"ST 2110-30 audio sender (L16 48kHz stereo)",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! "
         "audio/x-raw,format=S16BE,rate=48000,channels=2 ! "
         "rtpL16pay ! udpsink host=239.0.0.1 port=5006 "
         "auto-multicast=true sync=false",
         1280, 720, 30, 2});
    e->presets.push_back(
        {"ST 2110-30 audio receiver (L16 48kHz stereo)",
         "GStreamer In",
         "udpsrc address=239.0.0.1 port=5006 multicast-group=239.0.0.1 "
         "caps=\"application/x-rtp,media=audio,encoding-name=L16,"
         "clock-rate=48000,channels=2\" ! "
         "rtpL16depay ! audioconvert ! "
         "audio/x-raw,format=F32LE,rate=48000,channels=2 ! "
         "appsink name=audio sync=false"});

    // ST 2110-20 + 2110-30 combined sender
    e->presets.push_back(
        {"ST 2110-20/30 video+audio sender",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! "
         "video/x-raw,format=UYVP,width=1920,height=1080,framerate=30/1 ! "
         "rtpvrawpay ! udpsink host=239.0.0.1 port=5004 "
         "auto-multicast=true sync=false "
         "appsrc name=audio ! audioconvert ! "
         "audio/x-raw,format=S16BE,rate=48000,channels=2 ! "
         "rtpL16pay ! udpsink host=239.0.0.1 port=5006 "
         "auto-multicast=true sync=false",
         1920, 1080, 30, 2});

    // ST 2022-1 FEC: H.264 with forward error correction
    e->presets.push_back(
        {"ST 2022-1 H.264 sender (FEC)",
         "GStreamer Out",
         "appsrc name=video ! videoconvert ! x264enc tune=zerolatency ! "
         "rtph264pay ! rtpst2022-1-fecenc pt=96 ! "
         "udpsink host=239.0.0.1 port=5004 auto-multicast=true sync=false"});
    e->presets.push_back(
        {"ST 2022-1 H.264 receiver (FEC)",
         "GStreamer In",
         "udpsrc address=239.0.0.1 port=5004 multicast-group=239.0.0.1 "
         "caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96,"
         "clock-rate=90000\" ! rtpst2022-1-fecdec ! "
         "rtph264depay ! avdec_h264 ! videoconvert ! "
         "video/x-raw,format=RGBA ! appsink name=video sync=false"});

    // SMPTE 302M audio (broadcast PCM in MPEG-TS)
    e->presets.push_back(
        {"SMPTE 302M audio sender (MPEG-TS)",
         "GStreamer Out",
         "appsrc name=audio ! audioconvert ! "
         "audio/x-raw,format=S24LE,rate=48000,channels=2 ! "
         "avenc_s302m ! mpegtsmux ! "
         "udpsink host=239.0.0.1 port=5008 auto-multicast=true sync=false",
         1280, 720, 30, 2});

    enums.push_back({"Broadcast (ST 2110)", e});
  }

  return enums;
}

// Defined in GStreamerOutputDevice.cpp
Device::DeviceInterface* makeOutputDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx);

Device::DeviceInterface* ProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  auto set = settings.deviceSpecificSettings.value<GStreamerSettings>();
  if(pipeline_is_output(set.pipeline))
    return makeOutputDevice(settings, ctx);
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& ProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "GStreamer";
    GStreamerSettings specif;
    specif.pipeline
        = "videotestsrc name=src ! videoconvert "
          "! video/x-raw,format=RGBA ! appsink name=video";
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* ProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* ProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* ProtocolFactory::makeSettingsWidget()
{
  return new GStreamerSettingsWidget;
}

QVariant
ProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<GStreamerSettings>(visitor);
}

void ProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<GStreamerSettings>(data, visitor);
}

bool ProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::GStreamer::GStreamerSettings);

template <>
void DataStreamReader::read(const Gfx::GStreamer::GStreamerSettings& n)
{
  m_stream << n.pipeline << n.width << n.height << n.rate << n.audio_channels
           << n.input_transfer;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::GStreamer::GStreamerSettings& n)
{
  m_stream >> n.pipeline >> n.width >> n.height >> n.rate >> n.audio_channels
      >> n.input_transfer;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::GStreamer::GStreamerSettings& n)
{
  obj["Pipeline"] = n.pipeline;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Rate"] = n.rate;
  obj["AudioChannels"] = n.audio_channels;
  obj["InputTransfer"] = n.input_transfer;
}

template <>
void JSONWriter::write(Gfx::GStreamer::GStreamerSettings& n)
{
  n.pipeline = obj["Pipeline"].toString();
  n.width = obj["Width"].toInt();
  n.height = obj["Height"].toInt();
  n.rate = obj["Rate"].toInt();
  n.audio_channels = obj["AudioChannels"].toInt();
  if(auto v = obj.tryGet("InputTransfer"))
    n.input_transfer = v->toInt();
}
