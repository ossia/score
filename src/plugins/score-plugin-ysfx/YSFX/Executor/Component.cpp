// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include <Execution/DocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <YSFX/ProcessModel.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include <ossia/detail/ssize.hpp>

#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlContext>

#include <Scenario/Execution/score2OSSIA.hpp>

#include <vector>

namespace YSFX
{
namespace Executor
{
class ysfx_node final : public ossia::graph_node
{
public:
  ysfx_node(std::shared_ptr<ysfx_t>, ossia::execution_state& st);

  void
  run(const ossia::token_request& t,
      ossia::exec_state_facade) noexcept override;

  std::shared_ptr<ysfx_t> fx;
  ossia::execution_state& m_st;

  ossia::audio_inlet* audio_in{};
  ossia::audio_outlet* audio_out{};
  std::vector<ossia::value_port*> sliders;
};

Component::Component(
    YSFX::ProcessModel& proc,
    const ::Execution::Context& ctx,
    QObject* parent)
    : ::Execution::ProcessComponent_T<YSFX::ProcessModel, ossia::node_process>{
        proc,
        ctx,
        "YSFXComponent",
        parent}
{
  std::shared_ptr<ysfx_node> node = ossia::make_node<ysfx_node>(*ctx.execState, proc.fx, *ctx.execState);
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  int firstControlIndex = ysfx_get_num_inputs(proc.fx.get()) > 0 ? 1 : 0;
  for (std::size_t i = firstControlIndex, N = proc.inlets().size(); i < N; i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    // *node->controls[i - firstControlIndex].second
    //     = ossia::convert<float>(inlet->value());

    auto inl = node->sliders[i - firstControlIndex];
    connect(
        inlet,
        &Process::ControlInlet::valueChanged,
        this,
        [this, inl](const ossia::value& v) {
          system().executionQueue.enqueue([inl, val = v]() mutable {
            inl->write_value(std::move(val), 0);
          });
        });
  }

}

Component::~Component() { }

ysfx_node::ysfx_node(std::shared_ptr<ysfx_t> ffx, ossia::execution_state& st)
  : fx{std::move(ffx)}
  , m_st{st}
{
  ysfx_set_block_size(fx.get(), st.bufferSize);
  ysfx_set_sample_rate(fx.get(), st.sampleRate);
  if(ysfx_get_num_inputs(fx.get()) > 0)
  {
    this->m_inlets.push_back(audio_in = new ossia::audio_inlet);
  }

  if(ysfx_get_num_outputs(fx.get()) > 0)
  {
    this->m_outlets.push_back(audio_out = new ossia::audio_outlet);
  }

  for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
  {
    auto inl = new ossia::value_inlet;
    this->m_inlets.push_back(inl);
    this->sliders.push_back(&**inl);
    if (ysfx_slider_is_enum(fx.get(), i))
    {

    }
    else if(ysfx_slider_is_path(fx.get(), i))
    {

    }
    else
    {
      ysfx_slider_range_t range{};
      ysfx_slider_get_range(fx.get(), i, &range);

      (*inl)->domain = ossia::make_domain(range.min, range.max);
    }
  }
}

void ysfx_node::run(
    const ossia::token_request& tk,
    ossia::exec_state_facade estate) noexcept
{
  assert(fx);

  const auto [tick_start, d] = estate.timings(tk);

  double** ins{};
  int in_count{};
  if(audio_in)
  {
    in_count = this->audio_in->data.channels();
    if(in_count < (int)ysfx_get_num_inputs(this->fx.get()))
    {
      in_count = ysfx_get_num_inputs(this->fx.get());
      this->audio_in->data.set_channels(in_count);
    }
    ins = (double**) alloca(sizeof(double*) * in_count);
    for(int i = 0; i < in_count; i++) {
      {
        this->audio_in->data.get()[i].resize(d);
      ins[i] = this->audio_in->data.channel(i).data();
      }
    }
  }

  double** outs{};
  int out_count{};
  if(audio_out)
  {
    audio_out->data.set_channels(ysfx_get_num_outputs(this->fx.get()));
    out_count = this->audio_out->data.channels();
    outs = (double**) alloca(sizeof(double*) * out_count);
    for(int i = 0; i < out_count; i++) {

      this->audio_out->data.get()[i].resize(d);
      outs[i] = this->audio_out->data.channel(i).data();
    }
  }

  for (std::size_t i = 0; i < sliders.size(); i++)
  {
    auto& vp = *sliders[i];
    auto& dat = vp.get_data();

    if(!dat.empty())
    {
      auto& val = dat.back().value;
      if(float* f = val.target<float>())
      {
        ysfx_slider_set_value(fx.get(), i, *f);
      }
    }
  }

  ysfx_process_double(fx.get(), ins, outs, in_count, out_count, d);
}
}
}
