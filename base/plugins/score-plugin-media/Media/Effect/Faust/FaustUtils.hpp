#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/faust/faust_utils.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/domain/domain.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Dataflow/Port.hpp>

namespace Media
{
namespace Faust
{

template <typename Proc>
struct UI
{
  Proc& fx;
  ossia::nodes::faust_setup_ui<UI> glue{*this};

  UI(Proc& sfx) : fx{sfx}
  {
  }

  void openTabBox(const char* label)
  {
  }
  void openHorizontalBox(const char* label)
  {
  }
  void openVerticalBox(const char* label)
  {
  }
  void closeBox()
  {
  }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val)
  {
      qDebug() << "UI: " <<  key << val;
  }
  void
  addSoundfile(const char* label, const char* filename, Soundfile** sf_zone)
  {
  }

  void addButton(const char* label, FAUSTFLOAT* zone)
  {
    auto inl = new Process::Button{label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone)
  {
    auto inl = new Process::Toggle{bool(*zone), label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addVerticalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    auto inl = new Process::FloatSlider{min, max, init, label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addHorizontalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    // TODO spinbox ?
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
  {
  }

  void addVerticalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};

template <typename Proc>
struct UpdateUI
{
  Proc& fx;
  std::size_t i = 1;
  ossia::nodes::faust_setup_ui<UpdateUI> glue{*this};

  UpdateUI(Proc& sfx) : fx{sfx}
  {
  }

  void openTabBox(const char* label)
  {
  }
  void openHorizontalBox(const char* label)
  {
  }
  void openVerticalBox(const char* label)
  {
  }
  void closeBox()
  {
  }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val)
  {
      qDebug() << "UpdateUI: " <<  key << val;
  }
  void
  addSoundfile(const char* label, const char* filename, Soundfile** sf_zone)
  {
  }

  void addButton(const char* label, FAUSTFLOAT* zone)
  {
    if (i < fx.inlets().size())
    {
      if(auto inlet = dynamic_cast<Process::Button*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        delete fx.inlets()[i];
        auto inl = new Process::Button{label, id, &fx};
        fx.inlets()[i] = inl;
        // TODO REUSE ADDRESS ?
      }
    }
    else
    {
      auto inl = new Process::Button{label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    i++;
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone)
  {
    if (i < fx.inlets().size())
    {
      if(auto inlet = dynamic_cast<Process::Toggle*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
        inlet->setValue(bool(*zone));
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        delete fx.inlets()[i];
        auto inl = new Process::Toggle{bool(*zone), label, id, &fx};
        fx.inlets()[i] = inl;
        // TODO REUSE ADDRESS ?
      }
    }
    else
    {
      auto inl = new Process::Toggle{bool(*zone), label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    i++;
  }

  void addVerticalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    if (i < fx.inlets().size())
    {
      if(auto inlet = dynamic_cast<Process::FloatSlider*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(min, max));
        inlet->setValue(init);
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        delete fx.inlets()[i];
        auto inl = new Process::FloatSlider{min, max, init, label, id, &fx};
        fx.inlets()[i] = inl;
      }
    }
    else
    {
      auto inl = new Process::FloatSlider{min, max, init, label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    i++;
  }

  void addHorizontalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step)
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
  {
  }

  void addVerticalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};
}
}
