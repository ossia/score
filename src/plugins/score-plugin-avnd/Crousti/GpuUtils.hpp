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

template <avnd::geometry_port Field>
static void postprocess_geometry(Field& ctrl, score::gfx::Edge& edge)
{
  auto edge_sink = edge.sink;
  if(auto pnode = dynamic_cast<score::gfx::ProcessNode*>(edge_sink->node))
  {
    ossia::geometry_spec spc;

    {
      if(ctrl.dirty_mesh)
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
      ctrl.dirty_mesh = false;
    }
    auto it = std::find(edge_sink->node->input.begin(), edge_sink->node->input.end(), edge_sink);
    SCORE_ASSERT(it != edge_sink->node->input.end());
    int n = it - edge_sink->node->input.begin();
    pnode->process(n, spc);
  }
}

static void readbackInputBuffer(
    score::gfx::RenderList& renderer
    , QRhiResourceUpdateBatch& res
    , const score::gfx::Node& parent
    , std::vector<QRhiReadbackResult>& m_readbacks
    , int port_index
    , int buffer_index
    )
{
  // FIXME: instead of doing this we could do the readback in the
  // producer node and just read its bytearray once...
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
        auto buf = src_renderer->bufferForOutput(*edge->source);
        if(buf)
        {
          auto& readback = m_readbacks[buffer_index];
          readback = {};
          res.readBackBuffer(buf, 0, buf->size(), &readback);
        }
      }
      break;
    }
  }
}

static void uploadOutputBuffer(
    score::gfx::RenderList& renderer, int k, avnd::cpu_buffer auto& cpu_buf, QRhiResourceUpdateBatch& res,
    std::vector<std::pair<const score::gfx::Port*, QRhiBuffer*>> & m_buffers)
{
  if(cpu_buf.changed)
  {
    assert(m_buffers.size() > k);
    auto& [port, buf] = m_buffers[k];

    const auto bytesize = avnd::get_bytesize(cpu_buf);
    if(!buf)
    {
      if(bytesize > 0)
      {
        buf = renderer.state.rhi->newBuffer(
            QRhiBuffer::Dynamic
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
    res.updateDynamicBuffer(buf, 0, bytesize, avnd::get_bytes(cpu_buf));
    cpu_buf.changed = false;
  }
}


template<typename T>
struct buffer_inputs_storage;

template<typename T>
  requires (avnd::buffer_input_introspection<T>::size > 0)
struct buffer_inputs_storage<T>
{
  std::vector<QRhiReadbackResult> m_readbacks = std::vector<QRhiReadbackResult>(avnd::buffer_input_introspection<T>::size);

  void readInputBuffers(QRhi& rhi, auto& state)
  {
    if constexpr(avnd::buffer_input_introspection<T>::size > 0)
    {
      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      avnd::buffer_input_introspection<T>::for_all_n2(
          avnd::get_inputs<T>(state),
          [&]<typename Field, std::size_t N, std::size_t NField>
          (Field& t, avnd::predicate_index<N> np, avnd::field_index<NField> nf)
      {
        SCORE_ASSERT(N < m_readbacks.size());
        auto& readback = m_readbacks[N].data;
        t.buffer.bytes = reinterpret_cast<unsigned char*>(readback.data());
        t.buffer.bytesize = readback.size();
        t.buffer.changed = true;
      });
    }
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer,
      QRhiResourceUpdateBatch*& res,
      auto& state,
      auto& parent)
  {
    avnd::buffer_input_introspection<T>::for_all_n2(
        avnd::get_inputs<T>(state),
        [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      readbackInputBuffer(renderer, *res, parent, m_readbacks, nf, np);
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
  std::vector<std::pair<const score::gfx::Port*, QRhiBuffer*>> m_buffers;

  void
  createOutput(score::gfx::RenderList& renderer, score::gfx::Port& port, const avnd::cpu_buffer auto& cpu_buf)
  {
    auto& rhi = *renderer.state.rhi;
    QRhiBuffer* buffer = nullptr;
    if(const auto bs = avnd::get_bytesize(cpu_buf); bs > 0)
    {
      buffer = rhi.newBuffer(
          QRhiBuffer::Dynamic
          , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
          , bs);

      buffer->create();
    }

    m_buffers.emplace_back(&port, buffer);
  }

  void init(score::gfx::RenderList& renderer, auto& state, auto& parent)
  {
    // Init buffers for the outputs
    avnd::buffer_output_introspection<T>::for_all_n2(
        avnd::get_outputs<T>(state), [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      SCORE_ASSERT(parent.output.size() > nf);
      SCORE_ASSERT(parent.output[nf]->type == score::gfx::Types::Buffer);
      createOutput(renderer, *parent.output[nf], port.buffer);
    });
  }

  void upload(score::gfx::RenderList& renderer, auto& state, QRhiResourceUpdateBatch& res)
  {
    avnd::buffer_output_introspection<T>::for_all_n(
        avnd::get_outputs<T>(state), [&]<std::size_t N>(auto& t, avnd::predicate_index<N> idx) {
      uploadOutputBuffer(renderer, idx, t.buffer, res, m_buffers);
    });
  }

  void release()
  {
    // Free outputs
    for(auto& [p, buf] : m_buffers)
    {
      if(buf)
      {
        buf->destroy();
        buf->deleteLater();
      }
    }
    m_buffers.clear();
  }
};

template<typename T>
  requires (avnd::buffer_output_introspection<T>::size == 0)
struct buffer_outputs_storage<T>
{
  static void init(auto&&...)
  {

  }

  static void upload(auto&&...)
  {

  }

  static void release(auto&&...)
  {

  }
};

}

#endif
