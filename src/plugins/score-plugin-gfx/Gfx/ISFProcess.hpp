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

#include <algorithm>

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
      // Outlet id allocator for write-access storage / image inputs. Starts at
      // a high base so it never collides with inlet ids (input index `i`), the
      // default "Texture Out" outlet (id 1), or the MRT outlet base (10000).
      int& outlet_id;

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
        // Mirror the renderer (isf_input_port_vis in ISFNode.cpp): the access
        // qualifier decides inlet vs outlet. Treating every storage_input as a
        // read inlet gave write buffers a phantom TextureInlet — shifting every
        // later port by one (positional routing) and never exposing the
        // TextureOutlet the renderer actually produces.
        if(v.access == "read_only")
        {
          // read inlet: an upstream Buffer-producing node (ScenePreprocessor's
          // scene_* auxes, ExtractBuffer2 outputs, ...) has a target to land on.
          // For aux-named storage_inputs the RawRaster renderer also auto-binds
          // by name, so this inlet is optional but allows explicit wiring.
          auto port = new Gfx::TextureInlet(
              QString::fromStdString(input.name), Id<Process::Port>(i), &self);
          self.m_inlets.push_back(port);
          return port;
        }

        // write_only / read_write: the renderer pushes a Buffer OUTPUT port for
        // the produced SSBO so downstream nodes can connect to it.
        auto outport = new Gfx::TextureOutlet(
            QString::fromStdString(input.name), Id<Process::Port>(outlet_id++),
            &self);
        self.m_outlets.push_back(outport);

        // Conditional sizing inlet: only buffers whose layout ends in a
        // flexible-array member synthesize a "size" control — SAME condition as
        // CSF/Process.cpp setupCSF, the renderer, and the generated GLSL.
        if(!v.layout.empty()
           && v.layout.back().type.find("[]") != std::string::npos)
        {
          auto size_inl = new Process::IntSpinBox{
              1, 536870911, 1024,
              QString::fromStdString(input.name) + " size",
              Id<Process::Port>(i), &self};
          self.m_inlets.push_back(size_inl);
          self.controlAdded(size_inl->id());
          return size_inl;
        }
        return nullptr;
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
      Process::Inlet* operator()(const texture_input& v)
      {
        // The renderer (isf_input_port_vis) creates an Image input port for
        // every texture_input; returning nullptr here dropped the inlet and
        // shifted all subsequent ports (same off-by-one drift family as the
        // storage / csf_image cases).
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(i), &self);
        self.m_inlets.push_back(port);
        return port;
      }
      Process::Inlet* operator()(const csf_image_input& v)
      {
        // Mirror the renderer: read_only → input port (an upstream texture
        // cable lands on it); write_only / read_write → output port for the
        // produced storage image. Always creating an inlet gave write images a
        // phantom inlet (port shift) and no outlet for downstream connection.
        if(v.access == "read_only")
        {
          auto port = new Gfx::TextureInlet(
              QString::fromStdString(input.name), Id<Process::Port>(i), &self);
          self.m_inlets.push_back(port);
          return port;
        }
        auto outport = new Gfx::TextureOutlet(
            QString::fromStdString(input.name), Id<Process::Port>(outlet_id++),
            &self);
        self.m_outlets.push_back(outport);
        return nullptr;
      }
      Process::Inlet* operator()(const geometry_input& v) { return nullptr; }
    };

    // Outlet ids for write-access storage / image inputs. Base 20000 keeps
    // them clear of inlet ids (input index), the default outlet (id 1) and the
    // MRT base (10000), and lets the MRT block below tell them apart.
    static constexpr int storage_outlet_base = 20000;
    int outlet_id = storage_outlet_base;

    for(const isf::input& input : desc.inputs)
    {
      ossia::visit(input_vis{previous_values, input, i, self, outlet_id}, input.data);
      i++;
    }

    // The renderer (isf_input_port_vis) pushes write-storage / write-image
    // OUTPUT ports first (in input order), then the color / MRT outputs. The
    // model's outlets must follow the same order for positional routing. The
    // default "Texture Out" outlet was created by the constructor *before* this
    // loop, so it currently sits ahead of any storage outlets — pull the
    // storage outlets (ids >= storage_outlet_base) to the front to match.
    {
      std::stable_partition(
          self.m_outlets.begin(), self.m_outlets.end(),
          [](Process::Outlet* o) { return o->id().val() >= storage_outlet_base; });
    }

    // MRT: recreate the color outlets from OUTPUTS declarations. Preserve the
    // storage / image write outlets (ids >= storage_outlet_base); only the
    // color / default outlets are replaced.
    if(!desc.outputs.empty())
    {
      for(auto it = self.m_outlets.begin(); it != self.m_outlets.end();)
      {
        if((*it)->id().val() < storage_outlet_base)
        {
          delete *it;
          it = self.m_outlets.erase(it);
        }
        else
        {
          ++it;
        }
      }

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
