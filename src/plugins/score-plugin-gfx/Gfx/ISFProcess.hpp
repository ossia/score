#pragma once

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessFactory.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>

#include <score/tools/File.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QFile>

#include <isf.hpp>

namespace Gfx
{
struct ISFHelpers
{
  template <typename T>
  static Process::Descriptor descriptorFromISFFile(QString path)
  {
    auto base = Metadata<Process::Descriptor_k, T>::get();
    if(path.isEmpty())
      return base;

    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return base;

    try
    {
      auto [_, desc] = isf::parser::parse_isf_header(score::readFileAsString(f));
      if(desc.credits.starts_with("Automatically converted from "))
        desc.credits = desc.credits.substr(strlen("Automatically converted from "));
      else if(desc.credits.starts_with("by "))
        desc.credits = desc.credits.substr(strlen("by "));
      if(!desc.credits.empty())
        base.author = QString::fromStdString(desc.credits);
      if(!desc.description.empty())
        base.description = QString::fromStdString(desc.description);
      for(auto& cat : desc.categories)
        base.tags.push_back(QString::fromStdString(cat));
    }
    catch(...)
    {
    }

    return base;
  }

  template <typename T>
  static void setupISFModelPorts(
      T& self, const isf::descriptor& desc,
      const ossia::flat_map<QString, ossia::value>& previous_values)
  {
    /*
  {
    auto& [shader, error] = score::gfx::ShaderCache::get(
        m_processedProgram.vertex.toLatin1(), QShader::Stage::VertexStage);
    SCORE_ASSERT(error.isEmpty());
  }
  {
    auto& [shader, error] = score::gfx::ShaderCache::get(
        m_processedProgram.fragment.toLatin1(), QShader::Stage::FragmentStage);
    SCORE_ASSERT(error.isEmpty());
  }
  */

    int i = 0;
    using namespace isf;
    struct input_vis
    {
      const ossia::flat_map<QString, ossia::value>& previous_values;
      const isf::input& input;
      const int i;
      T& self;

      Process::Inlet* operator()(const float_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        auto port = new Process::FloatSlider(
            v.min, v.max, v.def, nm, Id<Process::Port>(i), &self);

        self.m_inlets.push_back(port);
        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == ossia::val_type::FLOAT)
          port->setValue(it->second);

        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const long_input& v)
      {
        auto nm = QString::fromStdString(input.name);

        std::vector<std::pair<QString, ossia::value>> alternatives;
        if(v.labels.size() == v.values.size())
        {
          // Sane, respectful example
          for(std::size_t value_idx = 0; value_idx < v.values.size(); value_idx++)
          {
            auto& val = v.values[value_idx];
            if(auto int_ptr = ossia::get_if<int64_t>(&val))
            {
              alternatives.emplace_back(
                  QString::fromStdString(v.labels[value_idx]), int(*int_ptr));
            }
            else if(auto dbl_ptr = ossia::get_if<double>(&val))
            {
              alternatives.emplace_back(
                  QString::fromStdString(v.labels[value_idx]), int(*dbl_ptr));
            }
            else
            {
              alternatives.emplace_back(
                  QString::fromStdString(v.labels[value_idx]), int(value_idx));
            }
          }
        }
        else
        {
          for(std::size_t value_idx = 0; value_idx < v.values.size(); value_idx++)
          {
            auto& val = v.values[value_idx];
            if(auto int_ptr = ossia::get_if<int64_t>(&val))
            {
              alternatives.emplace_back(QString::number(*int_ptr), int(*int_ptr));
            }
            else if(auto dbl_ptr = ossia::get_if<double>(&val))
            {
              alternatives.emplace_back(QString::number(*dbl_ptr), int(*dbl_ptr));
            }
            else if(auto str_ptr = ossia::get_if<std::string>(&val))
            {
              alternatives.emplace_back(
                  QString::fromStdString(*str_ptr), int(value_idx));
            }
          }
        }

        if(alternatives.empty())
        {
          alternatives.emplace_back("0", 0);
          alternatives.emplace_back("1", 1);
          alternatives.emplace_back("2", 2);
        }

        auto port = new Process::ComboBox(
            std::move(alternatives), (int)v.def, nm, Id<Process::Port>(i), &self);

        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == port->value().get_type())
          port->setValue(it->second);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const event_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        auto port = new Process::Button(nm, Id<Process::Port>(i), &self);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const bool_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        auto port = new Process::Toggle(v.def, nm, Id<Process::Port>(i), &self);

        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == port->value().get_type())
          port->setValue(it->second);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const point2d_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        ossia::vec2f min{-100., -100.};
        ossia::vec2f max{100., 100.};
        ossia::vec2f init{0.0, 0.0};
        if(v.def)
          std::copy_n(v.def->begin(), 2, init.begin());
        if(v.min)
          std::copy_n(v.min->begin(), 2, min.begin());
        if(v.max)
          std::copy_n(v.max->begin(), 2, max.begin());
        auto port = new Process::XYSpinboxes{
            min, max, init, false, nm, Id<Process::Port>(i), &self};

        auto& ctx = score::IDocument::documentContext(self);
        auto& device_plug = ctx.template plugin<Explorer::DeviceDocumentPlugin>();
        const Device::DeviceList& list = device_plug.list();
        QString firstWindowDeviceName;
        for(auto dev : list.devices())
        {
          if(auto win = qobject_cast<WindowDevice*>(dev))
          {
            firstWindowDeviceName = win->name();
            break;
          }
        }

        if(!firstWindowDeviceName.isEmpty())
        {
          if(nm.contains("iMouse"))
            port->setAddress(
                State::AddressAccessor{
                    State::Address{firstWindowDeviceName, {"cursor", "absolute"}}});
          else if(nm.contains("mouse", Qt::CaseInsensitive))
            port->setAddress(
                State::AddressAccessor{
                    State::Address{firstWindowDeviceName, {"cursor", "scaled"}}});
        }

        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == port->value().get_type())
          port->setValue(it->second);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const point3d_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        ossia::vec3f min{-100., -100., -100.};
        ossia::vec3f max{100., 100., 100.};
        ossia::vec3f init{0., 0., 0.};
        if(v.def)
          std::copy_n(v.def->begin(), 3, init.begin());
        if(v.min)
          std::copy_n(v.min->begin(), 3, min.begin());
        if(v.max)
          std::copy_n(v.max->begin(), 3, max.begin());
        auto port
            = new Process::XYZSpinboxes{min, max, init, nm, Id<Process::Port>(i), &self};

        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == port->value().get_type())
          port->setValue(it->second);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }

      Process::Inlet* operator()(const color_input& v)
      {
        auto nm = QString::fromStdString(input.name);
        ossia::vec4f init{0.5, 0.5, 0.5, 1.};
        if(v.def)
        {
          std::copy_n(v.def->begin(), 4, init.begin());
        }
        auto port = new Process::HSVSlider(
            init, QString::fromStdString(input.name), Id<Process::Port>(i), &self);

        if(auto it = previous_values.find(nm);
           it != previous_values.end()
           && it->second.get_type() == port->value().get_type())
          port->setValue(it->second);

        self.m_inlets.push_back(port);
        self.controlAdded(port->id());
        return port;
      }
      Process::Inlet* operator()(const image_input& v)
      {
        auto port = new Gfx::TextureInlet(Id<Process::Port>(i), &self);
        port->setName(QString::fromStdString(input.name));
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audio_input& v)
      {
        auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
        port->setName(QString::fromStdString(input.name));
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audioFFT_input& v)
      {
        auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
        port->setName(QString::fromStdString(input.name));
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audioHist_input& v)
      {
        auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
        port->setName(QString::fromStdString(input.name));
        self.m_inlets.push_back(port);
        return port;
      }
      
      // CSF-specific input handlers
      Process::Inlet* operator()(const storage_input& v) { return nullptr; }
      Process::Inlet* operator()(const texture_input& v) { return nullptr; }
      Process::Inlet* operator()(const csf_image_input& v) { return nullptr; }
    };

    for(const isf::input& input : desc.inputs)
    {
      ossia::visit(input_vis{previous_values, input, i, self}, input.data);
      i++;
    }
  }
};
}
