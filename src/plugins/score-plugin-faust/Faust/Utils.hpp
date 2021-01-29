#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/faust/faust_utils.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDebug>

namespace Faust
{

template <typename Proc, bool Synth>
struct UI : ::UI
{
  Proc& fx;

  UI(Proc& sfx) : fx{sfx} { }

  void openTabBox(const char* label) override { }
  void openHorizontalBox(const char* label) override { }
  void openVerticalBox(const char* label) override { }
  void closeBox() override { }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override { }
  void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override { }

  void addButton(const char* label, FAUSTFLOAT* zone) override
  {
    if constexpr(Synth)
    {
      using namespace std::literals;
      if (label == "Panic"sv || label == "gate"sv)
        return;
    }

    auto inl = new Process::Button{label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
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
      FAUSTFLOAT step) override
  {
    if constexpr(Synth)
    {
      using namespace std::literals;
      if (label == "gain"sv || label == "freq"sv || label == "sustain"sv)
        return;
    }

    auto inl = new Process::FloatSlider{min, max, init, label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addHorizontalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    // TODO spinbox ?
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
      override
  {
    auto inl = new Process::Bargraph{min, max, min, label, getStrongId(fx.outlets()), &fx};
    fx.outlets().push_back(inl);
  }

  void
  addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};

template <typename Proc, bool SetInit>
struct UpdateUI : ::UI
{
  Proc& fx;
  std::size_t i = 1;
  std::size_t o = 1;

  UpdateUI(Proc& sfx) : fx{sfx} { }

  void openTabBox(const char* label) override { }
  void openHorizontalBox(const char* label) override { }
  void openVerticalBox(const char* label) override { }
  void closeBox() override { }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
  {
    qDebug() << "UpdateUI: " << key << val;
  }
  void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override { }

  void addButton(const char* label, FAUSTFLOAT* zone) override
  {
    if (i < fx.inlets().size())
    {
      if (auto inlet = dynamic_cast<Process::Button*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        fx.controlRemoved(*fx.inlets()[i]);
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

  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
  {
    if (i < fx.inlets().size())
    {
      if (auto inlet = dynamic_cast<Process::Toggle*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
        if constexpr (SetInit)
          inlet->setValue(bool(*zone));
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        fx.controlRemoved(*fx.inlets()[i]);
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
      FAUSTFLOAT step) override
  {
    if (i < fx.inlets().size())
    {
      if (auto inlet = dynamic_cast<Process::FloatSlider*>(fx.inlets()[i]))
      {
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(min, max));
        if constexpr (SetInit)
          inlet->setValue(init);
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        fx.controlRemoved(*fx.inlets()[i]);
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
      FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
      override
  {
    if (o < fx.outlets().size())
    {
      if (auto outlet = dynamic_cast<Process::Bargraph*>(fx.outlets()[o]))
      {
        outlet->setCustomData(label);
        outlet->setDomain(ossia::make_domain(min, max));
      }
      else
      {
        auto id = fx.outlets()[o]->id();
        fx.controlRemoved(*fx.outlets()[o]);
        delete fx.outlets()[o];

        auto inl = new Process::Bargraph{min, max, min, label, id, &fx};
        fx.outlets()[o] = inl;
      }
    }
    else
    {
      auto inl = new Process::Bargraph{min, max, min, label, getStrongId(fx.outlets()), &fx};
      fx.outlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    o++;
  }


  void
  addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};
}
