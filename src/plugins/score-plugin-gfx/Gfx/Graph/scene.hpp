#pragma once

#include "graph.hpp"
#include "nodes.hpp"
#include "renderstate.hpp"

#include <memory>
/*
struct VideoGraph : Graph
{
  VideoGraph()
  {
    auto prod = new HAPNode;
    auto screen = new ScreenNode;

    auto ps = new Edge{prod->output[0], screen->input[0]};

    nodes.push_back(prod);
    nodes.push_back(screen);

    edges.push_back(ps);
  }
};

struct FunVideoGraph : Graph
{
  FunVideoGraph()
  {
    auto hap = new HAPNode;
    auto ffmpeg = new FFMPEGNode;
    auto prod = new ProductNode;
    auto screen = new ScreenNode;

    edges.push_back(new Edge{hap->output[0], prod->input[0]});
    edges.push_back(new Edge{ffmpeg->output[0], prod->input[1]});
    edges.push_back(new Edge{prod->output[0], screen->input[0]});

    nodes.push_back(prod);
    nodes.push_back(screen);
  }
};*/
/*
struct ExampleGraph : Graph
{
  ExampleGraph()
  {
    auto color = new ColorNode;
    auto noise = new NoiseNode;
    auto prod = new ProductNode;
    auto screen = new ScreenNode;

    auto cp = new Edge{color->output[0], prod->input[0]};
    auto np = new Edge{noise->output[0], prod->input[1]};
    auto ps = new Edge{prod->output[0], screen->input[0]};

    nodes.push_back(color);
    nodes.push_back(noise);
    nodes.push_back(prod);
    nodes.push_back(screen);

    edges.push_back(cp);
    edges.push_back(np);
    edges.push_back(ps);
  }
};

struct DualHeadGraph : Graph
{
  DualHeadGraph()
  {
    {
      auto color = new ColorNode;
      auto noise = new NoiseNode;
      auto prod = new ProductNode;
      auto screen = new ScreenNode;

      auto cp = new Edge{color->output[0], prod->input[0]};
      auto np = new Edge{noise->output[0], prod->input[1]};
      auto ps = new Edge{prod->output[0], screen->input[0]};

      nodes.push_back(color);
      nodes.push_back(noise);
      nodes.push_back(prod);
      nodes.push_back(screen);

      edges.push_back(cp);
      edges.push_back(np);
      edges.push_back(ps);
    }

    {
      auto color = new ColorNode;
      auto noise = new NoiseNode;
      auto prod = new ProductNode;
      auto screen = new ScreenNode;

      auto cp = new Edge{color->output[0], prod->input[0]};
      auto np = new Edge{noise->output[0], prod->input[1]};
      auto ps = new Edge{prod->output[0], screen->input[0]};

      nodes.push_back(color);
      nodes.push_back(noise);
      nodes.push_back(prod);
      nodes.push_back(screen);

      edges.push_back(cp);
      edges.push_back(np);
      edges.push_back(ps);
    }
  }
};

struct MinimalGraph : Graph
{
  MinimalGraph()
  {
    nodes.push_back(new ColorNode{});
    nodes.push_back(new ScreenNode{});

    auto color = nodes[0];
    auto screen = nodes[1];

    color->input[0]->value = ossia::vec4f{0.6, 0.3, 0.78, 1.};

    auto edge = new Edge{color->output[0], screen->input[0]};
    color->output[0]->edges.push_back(edge);
    screen->input[0]->edges.push_back(edge);

    edges.push_back(edge);
  }
};
*/
