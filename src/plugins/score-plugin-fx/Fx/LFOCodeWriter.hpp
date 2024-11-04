#pragma once
#include <string>
#include <Process/CodeWriter.hpp>
namespace Nodes
{

  struct code_writer : Process::AvndCodeWriter
    {
      using Process::AvndCodeWriter::AvndCodeWriter;
      std::string typeName() const noexcept override { return "LFO"; }
      std::string initializer() const noexcept override
      {
        auto val = [](auto in) {
          auto c = qobject_cast<const Process::ControlInlet*>(in);
          return ossia::convert<float>(c->value());
        };
        auto val_s = [](auto in) {
          auto c = qobject_cast<const Process::ControlInlet*>(in);
          auto str = ossia::convert<std::string>(c->value());
          if(str == "Sin")
            return 0;
          else if(str == "Triangle")
            return 1;
          else if(str == "Saw")
            return 2;
          else if(str == "Square")
            return 3;
          else if(str == "Sample & Hold")
            return 4;
          else if(str == "Noise 1")
            return 5;
          else if(str == "Noise 2")
            return 6;
          else if(str == "Noise 3")
            return 7;
          return 0;
        };

        std::string init_list;
        init_list += fmt::format(".freq = {{ {} }}, ", val(self.inlets()[0]));
        init_list += fmt::format(".ampl = {{ {} }}, ", val(self.inlets()[1]));
        init_list += fmt::format(".offset = {{ {} }}, ", val(self.inlets()[2]));
        init_list += fmt::format(".jitter = {{ {} }}, ", val(self.inlets()[3]));
        init_list += fmt::format(".phase = {{ {} }}, ", val(self.inlets()[4]));
        init_list += fmt::format(
            ".waveform = {{ ao::LFO::Inputs::Waveform::enum_type({}) }}, ",
            val_s(self.inlets()[5]));
        return fmt::format(".inputs = {{ {} }}, .setup = {{ .rate = 1e6 }}", init_list);
      }
    };
    }