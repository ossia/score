#pragma once

#include <avnd/introspection/gfx.hpp>
#if SCORE_PLUGIN_GFX
#include <Process/ExecutionContext.hpp>

#include <Crousti/File.hpp>
#include <Crousti/GppCoroutines.hpp>
#include <Crousti/GppShaders.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/TextureFormat.hpp>
#include <Crousti/TextureConversion.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <score/tools/ThreadPool.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCoreApplication>
#include <QTimer>
#include <QtGui/private/qrhi_p.h>

#include <ossia/detail/small_flat_map.hpp>
#include <avnd/binding/ossia/port_run_postprocess.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/binding/ossia/soundfiles.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <fmt/format.h>
#include <gpp/layout.hpp>

#include <score_plugin_avnd_export.h>

namespace oscr
{
struct GpuWorker
{
  template <typename T>
  void initWorker(this auto& self, std::shared_ptr<T>& state) noexcept
  {
    if constexpr(avnd::has_worker<T>)
    {
      auto ptr = QPointer{&self};
      auto& tq = score::TaskPool::instance();
      using worker_type = decltype(state->worker);

      auto wk_state = std::weak_ptr{state};
      state->worker.request = [ptr, &tq, wk_state]<typename... Args>(Args&&... f) {
        using type_of_result = decltype(worker_type::work(std::forward<Args>(f)...));
        tq.post([... ff = std::forward<Args>(f), wk_state, ptr]() mutable {
          if constexpr(std::is_void_v<type_of_result>)
          {
            worker_type::work(std::forward<decltype(ff)>(ff)...);
          }
          else
          {
            // If the worker returns a std::function, it
            // is to be invoked back in the processor DSP thread
            auto res = worker_type::work(std::forward<decltype(ff)>(ff)...);
            if(!res || !ptr)
              return;

            ossia::qt::run_async(
                QCoreApplication::instance(),
                [res = std::move(res), wk_state, ptr]() mutable {
              if(ptr)
                if(auto state = wk_state.lock())
                  res(*state);
                });
          }
        });
      };
    }
  }
};

template <typename GpuNodeRenderer, typename Node>
struct GpuProcessIns
{
  GpuNodeRenderer& gpu;
  Node& state;
  const score::gfx::Message& prev_mess;
  const score::gfx::Message& mess;
  const score::DocumentContext& ctx;

  bool can_process_message(std::size_t N)
  {
    if(mess.input.size() <= N)
      return false;

    if(prev_mess.input.size() == mess.input.size())
    {
      auto& prev = prev_mess.input[N];
      auto& next = mess.input[N];
      if(prev.index() == 1 && next.index() == 1)
      {
        if(ossia::get<ossia::value>(prev) == ossia::get<ossia::value>(next))
        {
          return false;
        }
      }
    }
    return true;
  }

  void operator()(avnd::parameter auto& t, auto field_index)
  {
    if(!can_process_message(field_index))
      return;

    if(auto val = ossia::get_if<ossia::value>(&mess.input[field_index]))
    {
      oscr::from_ossia_value(t, *val, t.value);
      if_possible(t.update(state));
    }
  }

#if OSCR_HAS_MMAP_FILE_STORAGE
  template <avnd::raw_file_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    // FIXME we should be loading a file there
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    using file_ports = avnd::raw_file_input_introspection<Node>;

    if(!can_process_message(field_index))
      return;

    auto val = ossia::get_if<ossia::value>(&mess.input[field_index]);
    if(!val)
      return;

    static constexpr bool has_text = requires { decltype(Field::file)::text; };
    static constexpr bool has_mmap = requires { decltype(Field::file)::mmap; };

    // First we can load it directly since execution hasn't started yet
    if(auto hdl = loadRawfile(*val, ctx, has_text, has_mmap))
    {
      static constexpr auto N = file_ports::field_index_to_index(NField);
      if constexpr(avnd::port_can_process<Field>)
      {
        // FIXME also do it when we get a run-time message from the exec engine,
        // OSC, etc
        auto func = executePortPreprocess<Field>(*hdl);
        const_cast<node_type&>(gpu.node())
            .file_loaded(
                state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
        if(func)
          func(state);
      }
      else
      {
        const_cast<node_type&>(gpu.node())
            .file_loaded(
                state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
      }
    }
  }
#endif

  template <avnd::buffer_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    auto& node = const_cast<node_type&>(gpu.node());
    auto val = ossia::get_if<ossia::render_target_spec>(&mess.input[field_index]);
    if(!val)
      return;
    node.process(NField, *val);
  }

  template <avnd::texture_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    auto& node = const_cast<node_type&>(gpu.node());
    auto val = ossia::get_if<ossia::render_target_spec>(&mess.input[field_index]);
    if(!val)
      return;
    node.process(NField, *val);
  }

  template <avnd::geometry_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    auto& node = const_cast<node_type&>(gpu.node());

    // FIXME
  }

  void operator()(auto& t, auto field_index) = delete;
};

struct GpuControlIns
{
  template <typename Self, typename Node_T>
  static void processControlIn(
      Self& self, Node_T& state, score::gfx::Message& renderer_mess,
      const score::gfx::Message& mess, const score::DocumentContext& ctx) noexcept
  {
    // Apply the controls
    avnd::input_introspection<Node_T>::for_all_n(
        avnd::get_inputs<Node_T>(state),
        GpuProcessIns<Self, Node_T>{self, state, renderer_mess, mess, ctx});
    renderer_mess = mess;
  }
};

struct GpuControlOuts
{
  std::weak_ptr<Execution::ExecutionCommandQueue> queue;
  Gfx::exec_controls control_outs;

  int64_t instance{};

  template <typename Node_T>
  void processControlOut(Node_T& state) const noexcept
  {
    if(!this->control_outs.empty())
    {
      auto q = this->queue.lock();
      if(!q)
        return;
      auto& qq = *q;
      int parm_k = 0;
      avnd::parameter_output_introspection<Node_T>::for_all(
          avnd::get_outputs(state), [&]<avnd::parameter T>(const T& t) {
            qq.enqueue([v = oscr::to_ossia_value(t, t.value),
                        port = control_outs[parm_k]]() mutable {
              std::swap(port->value, v);
              port->changed = true;
            });

            parm_k++;
          });
    }
  }
};

template <typename T>
struct SCORE_PLUGIN_AVND_EXPORT GpuNodeElements
{
  [[no_unique_address]] oscr::soundfile_storage<T> soundfiles;

  [[no_unique_address]] oscr::midifile_storage<T> midifiles;

#if defined(OSCR_HAS_MMAP_FILE_STORAGE)
  [[no_unique_address]] oscr::raw_file_storage<T> rawfiles;
#endif

  template <std::size_t N, std::size_t NField>
  void file_loaded(
      auto& state, const std::shared_ptr<oscr::raw_file_data>& hdl,
      avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    this->rawfiles.load(
        state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }
};

struct SCORE_PLUGIN_AVND_EXPORT CustomGfxNodeBase : score::gfx::NodeModel
{
  explicit CustomGfxNodeBase(const score::DocumentContext& ctx)
      : score::gfx::NodeModel{}
      , m_ctx{ctx}
  {
  }
  virtual ~CustomGfxNodeBase();
  const score::DocumentContext& m_ctx;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
  using score::gfx::NodeModel::process;
};
struct SCORE_PLUGIN_AVND_EXPORT CustomGfxOutputNodeBase : score::gfx::OutputNode
{
  virtual ~CustomGfxOutputNodeBase();

  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
};
struct CustomGpuNodeBase
    : score::gfx::Node
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
{
  CustomGpuNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue>&& q, Gfx::exec_controls&& ctls,
      const score::DocumentContext& ctx)
      : GpuControlOuts{std::move(q), std::move(ctls)}
      , m_ctx{ctx}
  {
  }

  virtual ~CustomGpuNodeBase() = default;

  const score::DocumentContext& m_ctx;
  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
};

struct SCORE_PLUGIN_AVND_EXPORT CustomGpuOutputNodeBase
    : score::gfx::OutputNode
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
{
  CustomGpuOutputNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls&& ctls,
      const score::DocumentContext& ctx);
  virtual ~CustomGpuOutputNodeBase();

  const score::DocumentContext& m_ctx;
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::function<void()> m_update;

  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
  using score::gfx::Node::process;

  void setRenderer(std::shared_ptr<score::gfx::RenderList>) override;
  score::gfx::RenderList* renderer() const override;

  void startRendering() override;
  void render() override;
  void stopRendering() override;
  bool canRender() const override;
  void onRendererChange() override;

  void createOutput(
      score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize) override;

  void destroyOutput() override;
  std::shared_ptr<score::gfx::RenderState> renderState() const override;

  Configuration configuration() const noexcept override;
};

template <typename Node_T, typename Node>
void prepareNewState(std::shared_ptr<Node_T>& eff, const Node& parent)
{
  if constexpr(avnd::has_worker<Node_T>)
  {
    parent.initWorker(eff);
  }
  if constexpr(avnd::has_processor_to_gui_bus<Node_T>)
  {
    auto& process = parent.processModel;
    eff->send_message = [ptr = QPointer{&process}](auto&& b) mutable {
      // FIXME right now all the rendering is done in the UI thread, which is very MEH
      //    this->in_edit([&process, bb = std::move(b)]() mutable {

      if(ptr && ptr->to_ui)
        MessageBusSender{ptr->to_ui}(std::move(b));
      //    });
    };

    // FIXME GUI -> engine. See executor.hpp
  }

  avnd::init_controls(*eff);

  if constexpr(avnd::can_prepare<Node_T>)
  {
    if constexpr(avnd::function_reflection<&Node_T::prepare>::count == 1)
    {
      using prepare_type = avnd::first_argument<&Node_T::prepare>;
      prepare_type t;
      if_possible(t.instance = parent.instance);
      eff->prepare(t);
    }
    else
    {
      eff->prepare();
    }
  }
}

struct port_to_type_enum
{
  template <std::size_t I, avnd::buffer_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Buffer;
  }

  template <std::size_t I, avnd::cpu_texture_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    using texture_type = std::remove_cvref_t<decltype(F::texture)>;
    return avnd::cpu_fixed_format_texture<texture_type> ? score::gfx::Types::Image
                                                        : score::gfx::Types::Buffer;
  }

  template <std::size_t I, avnd::sampler_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }
  template <std::size_t I, avnd::image_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }
  template <std::size_t I, avnd::attachment_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }

  template <std::size_t I, avnd::geometry_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Geometry;
  }
  template <std::size_t I, avnd::mono_audio_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Audio;
  }
  template <std::size_t I, avnd::poly_audio_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Audio;
  }
  template <std::size_t I, avnd::int_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Int;
  }
  template <std::size_t I, avnd::enum_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Int;
  }
  template <std::size_t I, avnd::float_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Float;
  }
  template <std::size_t I, avnd::parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    using value_type = std::remove_cvref_t<decltype(F::value)>;

    if constexpr(std::is_aggregate_v<value_type>)
    {
      constexpr int sz = boost::pfr::tuple_size_v<value_type>;
      if constexpr(sz == 2)
      {
        return score::gfx::Types::Vec2;
      }
      else if constexpr(sz == 3)
      {
        return score::gfx::Types::Vec3;
      }
      else if constexpr(sz == 4)
      {
        return score::gfx::Types::Vec4;
      }
    }
    return score::gfx::Types::Empty;
  }
  template <std::size_t I, typename F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Empty;
  }
};

template <typename Node_T>
inline void initGfxPorts(auto* self, auto& input, auto& output)
{
  avnd::input_introspection<Node_T>::for_all(
      [self, &input]<typename Field, std::size_t I>(avnd::field_reflection<I, Field> f) {
    static constexpr auto type = port_to_type_enum{}(f);
    input.push_back(new score::gfx::Port{self, {}, type, {}});
  });
  avnd::output_introspection<Node_T>::for_all(
      [self,
       &output]<typename Field, std::size_t I>(avnd::field_reflection<I, Field> f) {
    static constexpr auto type = port_to_type_enum{}(f);
    output.push_back(new score::gfx::Port{self, {}, type, {}});
  });
}

static QRhiBuffer* getInputBuffer(
    score::gfx::RenderList& renderer
    , const score::gfx::Node& parent
    , int port_index
    )
{
  const auto& inputs = parent.input;
  SCORE_ASSERT(port_index == 0);
  {
    score::gfx::Port* p = inputs[port_index];
    for(auto& edge : p->edges)
    {
      auto src_node = edge->source->node;
      score::gfx::NodeRenderer* src_renderer = src_node->renderedNodes.at(&renderer);
      if(src_renderer)
      {
        return src_renderer->bufferForOutput(*edge->source);
      }
      break;
    }
  }
  return nullptr;
}


static void readbackInputBuffer(
    score::gfx::RenderList& renderer
    , QRhiResourceUpdateBatch& res
    , const score::gfx::Node& parent
    , QRhiBufferReadbackResult& readback
    , int port_index
    )
{
  // FIXME: instead of doing this we could do the readback in the
  // producer node and just read its bytearray once...
  if(auto buf = getInputBuffer(renderer, parent, port_index))
  {
    readback = {};
    res.readBackBuffer(buf, 0, buf->size(), &readback);
  }
}

static void recreateOutputBuffer(
    score::gfx::RenderList& renderer, avnd::cpu_buffer auto& cpu_buf,
    QRhiResourceUpdateBatch& res, QRhiBuffer*& buf)
{
  const auto bytesize = avnd::get_bytesize(cpu_buf);
  if(!buf)
  {
    if(bytesize > 0)
    {
      buf = renderer.state.rhi->newBuffer(
          QRhiBuffer::Static
          , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
          , bytesize);

      buf->create();
    }
    else
    {
      cpu_buf.changed = false;
      return;
    }
  }
  else if(buf->size() != bytesize)
  {
    buf->destroy();
    buf->setSize(bytesize);
    buf->create();
  }
}

static void uploadOutputBuffer(
    score::gfx::RenderList& renderer, avnd::cpu_buffer auto& cpu_buf,
    QRhiResourceUpdateBatch& res, QRhiBuffer*& buf)
{
  if(cpu_buf.changed)
  {
    const auto bytesize = avnd::get_bytesize(cpu_buf);
    recreateOutputBuffer(renderer, cpu_buf, res, buf);
    score::gfx::uploadStaticBufferWithStoredData(&res, buf, 0, bytesize, (const char*)avnd::get_bytes(cpu_buf));
    cpu_buf.changed = false;
  }
}

static void uploadOutputBuffer(
    score::gfx::RenderList& renderer, avnd::gpu_buffer auto& cpu_buf,
    QRhiResourceUpdateBatch& res,  QRhiBuffer*& buf)
{
}


template<typename T>
struct buffer_inputs_storage;

template<typename T>
  requires (avnd::buffer_input_introspection<T>::size > 0)
struct buffer_inputs_storage<T>
{
  QRhiBufferReadbackResult m_readbacks[avnd::cpu_buffer_input_introspection<T>::size];
  QRhiBuffer* m_gpubufs[avnd::gpu_buffer_input_introspection<T>::size];

  void readInputBuffers(
      score::gfx::RenderList& renderer, auto& parent, auto& state)
  {
    if constexpr(avnd::cpu_buffer_input_introspection<T>::size > 0)
    {
      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      avnd::cpu_buffer_input_introspection<T>::for_all_n(
          avnd::get_inputs<T>(state),
          [&]<typename Field, std::size_t N>
          (Field& t, avnd::predicate_index<N> np)
      {
        auto& readback = m_readbacks[N].data;
        t.buffer.bytes = reinterpret_cast<unsigned char*>(readback.data());
        t.buffer.bytesize = readback.size();
        t.buffer.changed = true;
      });
    }

    if constexpr(avnd::gpu_buffer_input_introspection<T>::size > 0)
    {
      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      avnd::gpu_buffer_input_introspection<T>::for_all_n2(
          avnd::get_inputs<T>(state),
          [&]<typename Field, std::size_t N, std::size_t NField>
          (Field& t, avnd::predicate_index<N> np, avnd::field_index<NField> nf)
      {
        auto* buf = m_gpubufs[N];
        if(!buf)
          m_gpubufs[N] = getInputBuffer(renderer, parent, nf);
        buf = m_gpubufs[N];
        if(!buf)
           return;
        t.buffer.handle = buf;
        t.buffer.bytesize = buf->size();
        // t.buffer.changed = true;
      });
    }
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer,
      QRhiResourceUpdateBatch*& res,
      auto& state,
      auto& parent)
  {
    avnd::cpu_buffer_input_introspection<T>::for_all_n2(
        avnd::get_inputs<T>(state),
        [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      readbackInputBuffer(renderer, *res, parent, m_readbacks[N], nf);
    });
    avnd::gpu_buffer_input_introspection<T>::for_all_n2(
        avnd::get_inputs<T>(state),
        [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      m_gpubufs[N] = getInputBuffer(renderer, parent, nf);
    });
  }
};

template<typename T>
  requires (avnd::buffer_input_introspection<T>::size == 0)
struct buffer_inputs_storage<T>
{
  static void readInputBuffers(auto&&...)
  {

  }

  static void inputAboutToFinish(auto&&...)
  {

  }
};

template<typename T>
struct buffer_outputs_storage;

template<typename T>
  requires (avnd::buffer_output_introspection<T>::size > 0)
struct buffer_outputs_storage<T>
{
  std::pair<const score::gfx::Port*, QRhiBuffer*> m_buffers[avnd::buffer_output_introspection<T>::size];

  QRhiResourceUpdateBatch* currentResourceUpdateBatch{};
  std::pair<const score::gfx::Port*, QRhiBuffer*>
  createOutput(score::gfx::RenderList& renderer, score::gfx::Port& port, auto& buf)
  {
    auto& rhi = *renderer.state.rhi;
    QRhiBuffer* buffer = nullptr;
    if(const auto bs = avnd::get_bytesize(buf); bs > 0)
    {
      buffer = rhi.newBuffer(
          QRhiBuffer::Static
          , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
          , bs);

      buffer->create();
    }

    return {&port, buffer};
  }

  void init(score::gfx::RenderList& renderer, auto& state, auto& parent)
  {
    // Init buffers for the outputs
    avnd::buffer_output_introspection<T>::for_all_n2(
        avnd::get_outputs<T>(state), [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      SCORE_ASSERT(parent.output.size() > nf);
      SCORE_ASSERT(parent.output[nf]->type == score::gfx::Types::Buffer);

      if constexpr(requires { port.buffer.upload((const char*)nullptr, 1000, 0); })
      {
        auto& [gfx_port, buf] = m_buffers[N];
        gfx_port = parent.output[nf];
        buf = renderer.state.rhi->newBuffer(
            QRhiBuffer::Static
            , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
            , 1);

        buf->create();

        port.buffer.upload = [this, &renderer, &port] (const char* data, int64_t offset, int64_t bytesize) {
          SCORE_ASSERT(currentResourceUpdateBatch);
          auto& [gfx_port, buf] = m_buffers[N];

          if(!buf)
          {
            if(bytesize > 0)
            {
              buf = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static
                  , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
                  , bytesize);

              buf->create();
            }
            else
            {
              buf = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static
                  , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
                  , 1);

              buf->create();
              return;
            }
          }
          else if(buf->size() != bytesize)
          {
            buf->destroy();
            buf->setSize(bytesize);
            buf->create();
          }

          score::gfx::uploadStaticBufferWithStoredData(currentResourceUpdateBatch, buf, offset, bytesize, data);
        };
      }
      else
      {
        m_buffers[N] = createOutput(renderer, *parent.output[nf], port.buffer);
      }
    });
  }

  void prepareUpload(QRhiResourceUpdateBatch& res)
  {
    currentResourceUpdateBatch = &res;
  }

  void upload(score::gfx::RenderList& renderer, auto& state, QRhiResourceUpdateBatch& res)
  {
    avnd::buffer_output_introspection<T>::for_all_n(
        avnd::get_outputs<T>(state), [&]<std::size_t N>(auto& t, avnd::predicate_index<N> idx) {
      auto& [port, buf] = m_buffers[N];
      uploadOutputBuffer(renderer, t.buffer, res, buf);
    });
  }

  void release(score::gfx::RenderList& renderer)
  {
    // Free outputs
    for(auto& [p, buf] : m_buffers)
    {
      renderer.releaseBuffer(buf);
    }
  }
};

template<typename T>
  requires (avnd::buffer_output_introspection<T>::size == 0)
struct buffer_outputs_storage<T>
{
  static void init(auto&&...)
  {

  }

  static void prepareUpload(auto&&...)
  {
  }

  static void upload(auto&&...)
  {

  }

  static void release(auto&&...)
  {

  }
};


template <typename Tex>
static auto
createOutputTexture(score::gfx::RenderList& renderer, const Tex& texture_spec, QSize size)
{
  auto& rhi = *renderer.state.rhi;
  QRhiTexture* texture = &renderer.emptyTexture();
  if(size.width() > 0 && size.height() > 0)
  {
    texture = rhi.newTexture(
        gpp::qrhi::textureFormat(texture_spec), size, 1, QRhiTexture::Flag{});

    texture->create();
  }

  auto sampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

  sampler->create();
  return score::gfx::Sampler{sampler, texture};
}


template<typename T>
struct texture_inputs_storage;

template<typename T>
  requires (avnd::texture_input_introspection<T>::size > 0)
struct texture_inputs_storage<T>
{
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  QRhiReadbackResult m_readbacks[avnd::texture_input_introspection<T>::size];

  template <typename Tex>
  void createInput(
      score::gfx::RenderList& renderer, score::gfx::Port* port, const Tex& texture_spec,
      const score::gfx::RenderTargetSpecs& spec)
  {
    static constexpr auto flags
        = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
    auto texture = renderer.state.rhi->newTexture(
        gpp::qrhi::textureFormat(texture_spec), spec.size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port] = score::gfx::createRenderTarget(
        renderer.state, texture, renderer.samples(), renderer.requiresDepth());
  }

  void init(auto& self, score::gfx::RenderList& renderer)
  {
    // Init input render targets
    avnd::cpu_texture_input_introspection<T>::for_all_n(
        avnd::get_inputs<T>(*self.state),
        [&]<typename F, std::size_t K>(F& t, avnd::predicate_index<K>) {
      // FIXME k isn't the port index, it's the texture port index
      auto& parent = self.node();
      auto spec = parent.resolveRenderTargetSpecs(K, renderer);
      if constexpr(requires {
                     t.request_width;
                     t.request_height;
                   })
      {
        spec.size.rwidth() = t.request_width;
        spec.size.rheight() = t.request_height;
      }

      createInput(renderer, parent.input[K], t.texture, spec);

      if constexpr(avnd::cpu_fixed_format_texture<decltype(t.texture)>)
      {
        t.texture.width = spec.size.width();
        t.texture.height = spec.size.height();
      }
    });
  }

  void runInitialPasses(auto& self, QRhi& rhi)
  {
    // Fetch input textures (if any)
    // Copy the readback output inside the structure
    // TODO it would be much better to do this inside the readback's
    // "completed" callback.
    avnd::cpu_texture_input_introspection<T>::for_all_n(
        avnd::get_inputs<T>(*self.state), [&]<std::size_t K>(auto& t, avnd::predicate_index<K>) {
      oscr::loadInputTexture(rhi, m_readbacks, t.texture, K);
    });
  }

  void release()
  {
    // Free inputs
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();
  }

  void inputAboutToFinish(auto& parent, const score::gfx::Port& p,  QRhiResourceUpdateBatch*& res)
  {
    const auto& inputs = parent.input;
    auto index_of_port = ossia::find(inputs, &p) - inputs.begin();
    {
      auto tex = m_rts[&p].texture;
      auto& readback = m_readbacks[index_of_port];
      readback = {};
      res->readBackTexture(QRhiReadbackDescription{tex}, &readback);
    }
  }

};
template<typename T>
  requires (avnd::texture_input_introspection<T>::size == 0)
struct texture_inputs_storage<T>
{
  static void init(auto&&...) { }
  static void runInitialPasses(auto&&...) { }
  static void release(auto&&...) { }
  static void inputAboutToFinish(auto&&...) { }
};



template <avnd::cpu_texture Tex>
static QRhiTexture* updateTexture(auto& self, score::gfx::RenderList& renderer, int k, const Tex& cpu_tex)
{
  auto& [sampler, texture] = self.m_samplers[k];
  if(texture)
  {
    auto sz = texture->pixelSize();
    if(cpu_tex.width == sz.width() && cpu_tex.height == sz.height())
      return texture;
  }

  // Check the texture size
  if(cpu_tex.width > 0 && cpu_tex.height > 0)
  {
    QRhiTexture* oldtex = texture;
    QRhiTexture* newtex = renderer.state.rhi->newTexture(
        gpp::qrhi::textureFormat(cpu_tex), QSize{cpu_tex.width, cpu_tex.height}, 1,
        QRhiTexture::Flag{});
    newtex->create();
    for(auto& [edge, pass] : self.m_p)
      if(pass.srb)
        score::gfx::replaceTexture(*pass.srb, sampler, newtex);
    texture = newtex;

    if(oldtex && oldtex != &renderer.emptyTexture())
    {
      oldtex->deleteLater();
    }

    return newtex;
  }
  else
  {
    for(auto& [edge, pass] : self.m_p)
      if(pass.srb)
        score::gfx::replaceTexture(*pass.srb, sampler, &renderer.emptyTexture());

    return &renderer.emptyTexture();
  }
}

template <avnd::cpu_texture Tex>
static void uploadOutputTexture(auto& self,
                                score::gfx::RenderList& renderer, int k, Tex& cpu_tex,
                                QRhiResourceUpdateBatch* res)
{
  if(cpu_tex.changed)
  {
    if(auto texture = updateTexture(self, renderer, k, cpu_tex))
    {
      QByteArray buf
          = QByteArray::fromRawData((const char*)cpu_tex.bytes, cpu_tex.bytesize());
      if constexpr(requires { Tex::RGB; })
      {
        // RGB -> RGBA
        // FIXME other conversions
        const QByteArray rgb = buf;
        QByteArray rgba;
        rgba.resize(cpu_tex.width * cpu_tex.height * 4);
        auto src = (const unsigned char*)rgb.constData();
        auto dst = (unsigned char*)rgba.data();
        for(int rgb_byte = 0, rgba_byte = 0, N = rgb.size(); rgb_byte < N;)
        {
          dst[rgba_byte + 0] = src[rgb_byte + 0];
          dst[rgba_byte + 1] = src[rgb_byte + 1];
          dst[rgba_byte + 2] = src[rgb_byte + 2];
          dst[rgba_byte + 3] = 255;
          rgb_byte += 3;
          rgba_byte += 4;
        }
        buf = rgba;
      }

      // Upload it (mirroring is done in shader generic_texgen_fs if necessary)
      {
        QRhiTextureSubresourceUploadDescription sd(buf);
        QRhiTextureUploadDescription desc{QRhiTextureUploadEntry{0, 0, sd}};

        res->uploadTexture(texture, desc);
      }

      cpu_tex.changed = false;
    }
  }
}

static const constexpr auto generic_texgen_vs = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding=3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
#if defined(QSHADER_SPIRV) || defined(QSHADER_GLSL)
  v_texcoord = vec2(texcoord.x, 1. - texcoord.y);
#else
  v_texcoord = texcoord;
#endif
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";

static const constexpr auto generic_texgen_fs = R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;

void main ()
{
  fragColor = texture(y_tex, v_texcoord);
}
)_";

template<typename T>
struct texture_outputs_storage;

// If we have texture outs we need the whole rendering infrastructure
template<typename T>
  requires (avnd::texture_output_introspection<T>::size > 0)
struct texture_outputs_storage<T>
{
  void init(auto& self, score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
  {
    const auto& mesh = renderer.defaultTriangle();
    self.defaultMeshInit(renderer, mesh, res);
    self.processUBOInit(renderer);
    // Not needed here as we do not have a GPU pass:
    // this->m_material.init(renderer, this->node.input, this->m_samplers);

    std::tie(self.m_vertexS, self.m_fragmentS)
        = score::gfx::makeShaders(renderer.state, generic_texgen_vs, generic_texgen_fs);

    avnd::cpu_texture_output_introspection<T>::for_all(
        avnd::get_outputs<T>(*self.state), [&](auto& t) {
      self.m_samplers.push_back(
          createOutputTexture(renderer, t.texture, QSize{t.texture.width, t.texture.height}));
    });

    self.defaultPassesInit(renderer, mesh);
  }

  void runInitialPasses(auto& self,
                        score::gfx::RenderList& renderer,
                        QRhiResourceUpdateBatch*& res)
  {
    avnd::cpu_texture_output_introspection<T>::for_all_n(
        avnd::get_outputs<T>(*self.state), [&]<std::size_t N>(auto& t, avnd::predicate_index<N>) {
      uploadOutputTexture(self, renderer, N, t.texture, res);
    });
  }

  void release(auto& self, score::gfx::RenderList& r)
  {
    // Free outputs
    for(auto& [sampl, texture] : self.m_samplers)
    {
      if(texture != &r.emptyTexture())
        texture->deleteLater();
      texture = nullptr;
    }
  }

};

template<typename T>
  requires (avnd::texture_output_introspection<T>::size == 0)
struct texture_outputs_storage<T>
{
  static void init(auto& self, score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
  {
  }

  static void runInitialPasses(auto& self,
                        score::gfx::RenderList& renderer,
                        QRhiResourceUpdateBatch*& res)
  {
  }

  static void release(auto& self, score::gfx::RenderList& r)
  {
  }
};
template<typename T>
struct geometry_outputs_storage;

template<typename T>
  requires (avnd::geometry_output_introspection<T>::size > 0)
struct geometry_outputs_storage<T>
{
  ossia::geometry_spec specs[avnd::geometry_output_introspection<T>::size];

  template <avnd::geometry_port Field>
  void reload_mesh(Field& ctrl, ossia::geometry_spec& spc)
  {
    spc.meshes = std::make_shared<ossia::mesh_list>();
    auto& ossia_meshes = *spc.meshes;
    if constexpr(avnd::static_geometry_type<Field> || avnd::dynamic_geometry_type<Field>)
    {
      ossia_meshes.meshes.resize(1);
      load_geometry(ctrl, ossia_meshes.meshes[0]);
    }
    else if constexpr(
        avnd::static_geometry_type<decltype(Field::mesh)>
        || avnd::dynamic_geometry_type<decltype(Field::mesh)>)
    {
      ossia_meshes.meshes.resize(1);
      load_geometry(ctrl.mesh, ossia_meshes.meshes[0]);
    }
    else
    {
      load_geometry(ctrl, ossia_meshes);
    }
  }

  template <avnd::geometry_port Field, std::size_t N>
  void upload(
      score::gfx::RenderList& renderer, Field& ctrl, score::gfx::Edge& edge,
      avnd::predicate_index<N>)
  {
    auto edge_sink = edge.sink;
    if(auto pnode = dynamic_cast<score::gfx::ProcessNode*>(edge_sink->node))
    {
      ossia::geometry_spec& spc = specs[N];

      // 1. Reload mesh
      {
        if(ctrl.dirty_mesh)
        {
          reload_mesh(ctrl, spc);
        }
        else
        {
          if(spc.meshes)
          {
            auto& ossia_meshes = *spc.meshes;

            bool need_reload = false;
            if constexpr(avnd::static_geometry_type<Field> || avnd::dynamic_geometry_type<Field>)
            {
              SCORE_ASSERT(ossia_meshes.meshes.size() == 1);
              need_reload = update_geometry(ctrl, ossia_meshes.meshes[0]);
            }
            else if constexpr(
                avnd::static_geometry_type<decltype(Field::mesh)>
                || avnd::dynamic_geometry_type<decltype(Field::mesh)>)
            {
              SCORE_ASSERT(ossia_meshes.meshes.size() == 1);
              need_reload = update_geometry(ctrl.mesh, ossia_meshes.meshes[0]);
            }
            else
            {
              need_reload = update_geometry(ctrl, ossia_meshes);
            }

            if(need_reload)
            {
              reload_mesh(ctrl, spc);
            }
          }
        }
        ctrl.dirty_mesh = false;
      }

      // 2. Push to next node
      // FIXME this should be for the renderer of edge, not the node, since
      // geometries can have gpu buffers
      auto rendered_node = pnode->renderedNodes.find(&renderer);
      SCORE_ASSERT(rendered_node != pnode->renderedNodes.end());

      auto it = std::find(
          edge_sink->node->input.begin(), edge_sink->node->input.end(), edge_sink);
      SCORE_ASSERT(it != edge_sink->node->input.end());
      int n = it - edge_sink->node->input.begin();

      rendered_node->second->process(n, spc);

      // 3. Same for transform3d

      if constexpr(requires { ctrl.transform; })
      {
        if(ctrl.dirty_transform)
        {
          ossia::transform3d transform;
          std::copy_n(ctrl.transform, std::ssize(ctrl.transform), transform.matrix);
          ctrl.dirty_transform = false;
          pnode->process(n, transform);
        }
      }
    }
  }

  void upload(score::gfx::RenderList& renderer, auto& state, score::gfx::Edge& edge)
  {
    // FIXME we need something such as port_run_{pre,post}process for GPU nodes
    avnd::geometry_output_introspection<T>::for_all_n(
        avnd::get_outputs(state),
        [&](auto& field, auto pred) { this->upload(renderer, field, edge, pred); });
  }
};


template<typename T>
  requires (avnd::geometry_output_introspection<T>::size == 0)
struct geometry_outputs_storage<T>
{
  static void upload(auto&&...)
  {

  }
};
}

#endif
