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

        // Numeric mode: MIN/MAX set and no VALUES/LABELS → IntSpinBox
        if(v.values.empty() && v.min && v.max)
        {
          auto port = new Process::IntSpinBox(
              *v.min, *v.max, (int)v.def, nm, Id<Process::Port>(i), &self);

          if(auto it = previous_values.find(nm);
             it != previous_values.end()
             && it->second.get_type() == port->value().get_type())
            port->setValue(it->second);

          self.m_inlets.push_back(port);
          self.controlAdded(port->id());
          return port;
        }

        // Enum mode: VALUES/LABELS → ComboBox
        std::vector<std::pair<QString, ossia::value>> alternatives;
        if(v.labels.size() == v.values.size())
        {
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

        // ComboBox::init expects the VALUE to be initially selected, not
        // an index. libisf's `v.def` is the INDEX into values for enum
        // mode — passing it raw was making `DEFAULT: <value>` silently
        // fall back to alternatives[0] when <value> didn't equal a valid
        // index. Look up the alternative at v.def and forward its value.
        // Same fix lives in CSF/Process.cpp + GeometryFilter/Process.cpp.
        const std::size_t def_idx
            = std::min<std::size_t>(v.def, alternatives.size() - 1);
        const ossia::value& init_value = alternatives[def_idx].second;

        auto port = new Process::ComboBox(
            std::move(alternatives), init_value, nm, Id<Process::Port>(i), &self);

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
                    State::Address{firstWindowDeviceName, {"cursor", "gl"}}});
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
        auto port = new Process::XYZSpinboxes{
            min, max, init, false, nm, Id<Process::Port>(i), &self};

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
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);

        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const cubemap_input& v)
      {
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);

        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audio_input& v)
      {
        auto port = new Process::AudioInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audioFFT_input& v)
      {
        auto port = new Process::AudioInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const audioHist_input& v)
      {
        auto port = new Process::AudioInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      
      // CSF-specific input handlers
      Process::Inlet* operator()(const storage_input& v)
      {
        // storage_input declares an SSBO the shader reads from. Create a
        // Process-level TextureInlet so an upstream Buffer-producing node
        // (ScenePreprocessor's scene_* auxes extracted via ExtractBuffer2, etc.)
        // has a target to connect to. Note that for
        // aux-named storage_inputs (scene_lights, scene_materials, per_draw),
        // the RawRaster renderer auto-binds from the upstream geometry's
        // auxiliary_buffer[] by matching on name — so this inlet is
        // optional for those; leaving it exposed makes explicit wiring
        // possible when no preprocessor sits upstream.
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const uniform_input& v)
      {
        // uniform_input expects an upstream Buffer port (ScenePreprocessor's
        // camera/env aux buffers, ExtractBuffer2 outputs, etc.). TextureInlet
        // is score's Process-layer inlet for SSBO / texture / UBO data flow.
        // Without this, the Process model has no inlet for the cable to land
        // on and Score.inlet(proc, i) returns null.
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const texture_input& v) { return nullptr; }
      Process::Inlet* operator()(const csf_image_input& v)
      {
        // csf_image_input is a storage image bound to the graphics pipeline
        // (vertex / fragment) for imageLoad / imageStore. Like uniform_input
        // and the existing image samplers, it needs a Process-layer inlet so
        // that an upstream texture cable can land on it (e.g. read_only
        // images sourced from another node's output, or scratch images that
        // ping-pong with an upstream allocator). TextureInlet is the generic
        // GPU-resource port used elsewhere in score for textures / SSBOs.
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const geometry_input& v) { return nullptr; }
    };

    for(const isf::input& input : desc.inputs)
    {
      ossia::visit(input_vis{previous_values, input, i, self}, input.data);
      i++;
    }

    // MRT: recreate outlets from OUTPUTS declarations
    if(!desc.outputs.empty())
    {
      qDeleteAll(self.m_outlets);
      self.m_outlets.clear();

      int outId = 10000; // High base to avoid ID collisions with inlets
      for(const auto& out : desc.outputs)
      {
        self.m_outlets.push_back(new Gfx::TextureOutlet{
            QString::fromStdString(out.name),
            Id<Process::Port>(outId++), &self});
      }
    }
  }
};
}
