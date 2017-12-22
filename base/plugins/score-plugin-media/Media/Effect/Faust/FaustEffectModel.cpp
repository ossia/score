#include "FaustEffectModel.hpp"
#include <QVBoxLayout>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <Media/Commands/EditFaustEffect.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <faust/dsp/llvm-dsp.h>
#include <faust/gui/UI.h>
#include <faust/gui/PathBuilder.h>
#include <ossia/network/domain/domain.hpp>
#include <score/tools/IdentifierGeneration.hpp>
namespace Media::Faust
{
class FaustControlInlet : public Process::ControlInlet
{
public:
  using Process::ControlInlet::ControlInlet;
  FAUSTFLOAT* zone{};
};
class FaustUI final : public PathBuilder, public UI
{
  FaustEffectModel& fx;

public:
  FaustUI(FaustEffectModel& sfx):
    fx{sfx}
  {
  }

private:
  void openTabBox(const char* label) override
  {
  }
  void openHorizontalBox(const char* label) override
  {
  }
  void openVerticalBox(const char* label) override
  {
  }
  void closeBox() override
  {
  }

  void addButton(const char* label, FAUSTFLOAT* zone) override
  {
    auto inl = new FaustControlInlet{getStrongId(fx.inlets()), &fx};
    inl->setCustomData(label);
    inl->zone = zone;
    fx.m_inlets.push_back(inl);
/*
    auto n = m_curNode->create_child(label);
    auto a = n->create_parameter(ossia::val_type::BOOL);
    a->add_callback([zone] (const ossia::value& val) {
      *zone = val.get<bool>() ? 1.0 : 0.0;
    });
    */
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
  {
    addButton(label, zone);
  }

  void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    auto inl = new FaustControlInlet{getStrongId(fx.inlets()), &fx};
    inl->setCustomData(label);
    inl->zone = zone;
    inl->setDomain(ossia::make_domain(min, max));
    inl->setValue(init);
    fx.m_inlets.push_back(inl);
    /*
    auto n = m_curNode->create_child(label);
    auto a = n->create_parameter(ossia::val_type::FLOAT);
    ossia::net::set_default_value(*n, init);
    ossia::net::set_domain(*n, ossia::make_domain(min, max));
    ossia::net::set_value_step_size(*n, step);
    a->add_callback([zone] (const ossia::value& val) {
      *zone = val.get<float>();
    });
    */
  }

  void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    /*
    auto n = m_curNode->create_child(label);
    auto a = n->create_parameter(ossia::val_type::FLOAT);
    ossia::net::set_domain(*n, ossia::make_domain(min, max));
    ossia::net::set_access_mode(*n, ossia::access_mode::GET);

    m_values.push_back({a, zone});
    */
  }

  void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    addHorizontalBargraph(label, zone, min, max);
  }


  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
  {
    std::cout << "declare key : " << key << " val : " << val << std::endl;
  }

  void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
  {

  }
};


struct FaustEditDialog : public QDialog
{
  const FaustEffectModel& m_effect;

  QPlainTextEdit* m_textedit{};
public:
  FaustEditDialog(const FaustEffectModel& fx):
    m_effect{fx}
  {
    auto lay = new QVBoxLayout;
    this->setLayout(lay);

    m_textedit = new QPlainTextEdit{m_effect.text()};

    lay->addWidget(m_textedit);
    auto bbox = new QDialogButtonBox{
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    lay->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
  }

  QString text() const
  {
    return m_textedit->document()->toPlainText();
  }
};

FaustEffectModel::FaustEffectModel(
    const QString& faustProgram,
    const Id<EffectModel>& id,
    QObject* parent):
  Process::EffectModel{id, parent}
{
  setText(faustProgram);
  init();
  setText(R"__(
  import("stdfaust.lib");

  phasor(f)   = f/ma.SR : (+,1.0:fmod) ~ _ ;
  osc(f)      = phasor(f) * 6.28318530718 : sin;
  process     = osc(hslider("freq", 440, 20, 20000, 1)) * hslider("level", 0, 0, 1, 0.01);
)__");
}

FaustEffectModel::~FaustEffectModel()
{

}

void FaustEffectModel::setText(const QString& txt)
{
  m_text = txt;
  reload();
}

void FaustEffectModel::init()
{
  // We have to reload the faust FX whenever
  // some soundcard settings changes
  /*
    auto& ctx = score::AppComponents().applicationPlugin<Media::MediaStreamEngine::ApplicationPlugin>();
    con(ctx, &MediaStreamEngine::ApplicationPlugin::audioEngineRestarting,
        this, [this] () {
        saveParams();
    });
    con(ctx, &MediaStreamEngine::ApplicationPlugin::audioEngineRestarted,
            this, [this] () {
        reload();
    });
    */
}

QString FaustEffectModel::prettyName() const
{
  return "Faust";
}
void FaustEffectModel::reload()
{
  auto fx_text = m_text.toLocal8Bit();
  if(fx_text.isEmpty())
  {
    return;
  }
  std::string err;
  int argc{};
  char** argv{};
  auto fac = createDSPFactoryFromString(
        "score", fx_text.toStdString(),
        0, nullptr, "", err, -1);

  qDebug() << err.c_str();
  if(!fac)
    return;
  if(faust_factory)
    deleteDSPFactory(faust_factory);

  if(faust_object)
    delete faust_object;

  faust_factory = fac;
  faust_object = fac->createDSPInstance();
  faust_object->init(44100);

  m_inlets.push_back(new Process::Inlet{getStrongId(inlets()), this});
  m_inlets.back()->type = Process::PortType::Audio;
  m_outlets.push_back(new Process::Outlet{getStrongId(inlets()), this});
  m_outlets.back()->type = Process::PortType::Audio;
  if(faust_object->getNumInputs() > 0)
  {
  }
  else
  {
  }

  if(faust_object->getNumOutputs() > 0)
  {
  }
  else
  {
  }

  FaustUI ui{*this};
  faust_object->buildUserInterface(&ui);

  /*
  if(m_effect)
  {
    auto json = GetJsonEffect(m_effect);
    QJsonParseError err;
    auto qjs = QJsonDocument::fromJson(json, &err);
    if(err.error == QJsonParseError::NoError)
    {
      metadata().setLabel(qjs.object()["name"].toString());
    }
    else
    {
      qDebug() << err.errorString();
    }

    restoreParams();
  }
  else
  {
    qDebug() << "could not load effect";
  }

  */
}

void FaustEffectModel::showUI()
{
  FaustEditDialog edit{*this};
  auto res = edit.exec();
  if(res)
  {
    //CommandDispatcher<>{}.submitCommand(new Media::Commands::EditFaustEffect{*faust, edit.text()});
  }
}

void FaustEffectModel::hideUI()
{

}


}

template <>
void DataStreamReader::read(
    const Media::Faust::FaustEffectModel& eff)
{
}

template <>
void DataStreamWriter::write(
    Media::Faust::FaustEffectModel& eff)
{
}

template <>
void JSONObjectReader::read(
    const Media::Faust::FaustEffectModel& eff)
{
}

template <>
void JSONObjectWriter::write(
    Media::Faust::FaustEffectModel& eff)
{
}

namespace Engine::Execution
{

class faust_node : public ossia::audio_fx_node
{
    llvm_dsp* m_dsp{};
    class FaustExecUI final : public PathBuilder, public UI
    {
      faust_node& fx;

    public:
      FaustExecUI(faust_node& sfx):
        fx{sfx}
      {
      }

    private:
      void openTabBox(const char* label) override
      {
      }
      void openHorizontalBox(const char* label) override
      {
      }
      void openVerticalBox(const char* label) override
      {
      }
      void closeBox() override
      {
      }

      void addButton(const char* label, FAUSTFLOAT* zone) override
      {
        fx.inputs().push_back(ossia::make_inlet<ossia::value_port>());
        fx.controls.push_back({fx.inputs().back()->data.target<ossia::value_port>(), zone});
      }

      void addCheckButton(const char* label, FAUSTFLOAT* zone) override
      {
        addButton(label, zone);
      }

      void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
      {
        fx.inputs().push_back(ossia::make_inlet<ossia::value_port>());
        fx.controls.push_back({fx.inputs().back()->data.target<ossia::value_port>(), zone});
      }

      void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
      {
        addVerticalSlider(label, zone, init, min, max, step);
      }

      void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
      {
        addVerticalSlider(label, zone, init, min, max, step);
      }

      void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
      {
        fx.outputs().push_back(ossia::make_outlet<ossia::value_port>());
      }

      void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
      {
        addHorizontalBargraph(label, zone, min, max);
      }


      void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
      {
        std::cout << "declare key : " << key << " val : " << val << std::endl;
      }

      void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
      {

      }
    };
  public:
    ossia::small_vector<std::pair<ossia::value_port*, FAUSTFLOAT*>, 8> controls;
    faust_node(llvm_dsp& dsp):
      m_dsp{&dsp}
    {
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
      FaustExecUI ex{*this};
      m_dsp->buildUserInterface(&ex);
    }

    void run(ossia::token_request tk, ossia::execution_state&) override
    {
      auto d = tk.date - this->m_prev_date;
      if(d > 0)
      {
        for(auto ctrl : controls)
        {
          auto& dat = ctrl.first->get_data();
          if(!dat.empty())
          {
            *ctrl.second = ossia::convert<float>(dat.back().value);
          }
        }

        auto& audio_in = *m_inlets[0]->data.target<ossia::audio_port>();
        auto& audio_out = *m_outlets[0]->data.target<ossia::audio_port>();

        const auto n_in = m_dsp->getNumInputs();
        const auto n_out = m_dsp->getNumOutputs();

        float* inputs_ = (float*)alloca(n_in * d * sizeof(float));
        float* outputs_ = (float*)alloca(n_out * d * sizeof(float));

        float** input_n = (float**)alloca(sizeof(float*) * n_in);
        float** output_n = (float**)alloca(sizeof(float*) * n_out);

        // Copy inputs
        // TODO offset !!!
        for(int i = 0; i < n_in; i++)
        {
          input_n[i] = inputs_ + i * d;
          if(audio_in.samples.size() > i)
          {
            auto num_samples = std::min((std::size_t)d, (std::size_t)audio_in.samples[i].size());
            for(std::size_t j = 0; j < num_samples; j++)
            {
              input_n[i][j] = (float)audio_in.samples[i][j];
            }

            if(d > audio_in.samples[i].size())
            {
              for(std::size_t j = audio_in.samples[i].size(); j < d; j++)
              {
                input_n[i][j] = 0.f;
              }
            }
          }
          else
          {
            for(std::size_t j = 0; j < d; j++)
            {
              input_n[i][j] = 0.f;
            }
          }
        }

        for(int i = 0; i < n_out; i++)
        {
          output_n[i] = outputs_ + i * d;
          for(int j = 0; j < d; j++)
          {
            output_n[i][j] = 0.f;
          }
        }
        m_dsp->compute(d, input_n, output_n);

        audio_out.samples.resize(n_out);
        for(int i = 0; i < n_out; i++)
        {
          audio_out.samples[i].resize(d);
          for(int j = 0; j < d; j++)
          {
            audio_out.samples[i][j] = (double)output_n[i][j];
          }
        }

        // TODO handle multichannel cleanly
        if(n_out == 1)
        {
          audio_out.samples.resize(2);
          audio_out.samples[1] = audio_out.samples[0];
        }
      }
    }

    std::string_view label() const override
    {
      return "Faust";
    }

    void all_notes_off() override
    {
    }
};
Engine::Execution::FaustEffectComponent::FaustEffectComponent(
    Media::Faust::FaustEffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : Engine::Execution::EffectComponent_T<Media::Faust::FaustEffectModel>{proc, ctx, id, parent}
{
  if(proc.faust_object)
    node = std::make_shared<faust_node>(*proc.faust_object);

  for(int i = 1; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    connect(inlet, &Process::ControlInlet::valueChanged,
            this, [=] (const ossia::value& v) {
      system().executionQueue.enqueue([inlet=this->node->inputs()[i],val=v] () mutable {
        inlet->data.target<ossia::value_port>()->add_value(std::move(val), 0);
      });
    });
  }
}

}
