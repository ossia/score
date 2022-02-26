#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Concepts.hpp>
#include <rnd/random.hpp>

namespace examples
{
struct TextureFilterExample
{
  meta_attribute(pretty_name, "My example texture filter");
  meta_attribute(script_name, texture_filt);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "3183d03e-9228-4d50-98e0-e7601dd16a2e");

  struct {
    struct {
      meta_attribute(name, "In");

      // For inputs we have to request a specific size.
      // The bytes will be provided by an outside mechanism.
      oscr::rgba_texture texture{.width = 500, .height = 500};
    } image;
  } inputs;

  struct {
    struct {
      meta_attribute(name, "Out");

      oscr::rgba_texture texture;
    } image;
  } outputs;

  // Some place in RAM to store our pixels
  boost::container::vector<unsigned char> bytes;

  // Some state that will stay around, just to make our input texture move a bit
  int k = 0;

  // Some initialization can be done in the constructor.
  TextureFilterExample()
  {
    // Allocate some initial data
    bytes = oscr::rgba_texture::allocate(500, 500);
    outputs.image.texture.update(bytes.data(), 500, 500);
  }

  void operator()()
  {
    auto& in_tex = inputs.image.texture;

    // Since GPU readbacks are asynchronous: reading textures may take some time and
    // thus the data may not be available from the beginning.
    if(in_tex.bytes == nullptr)
      return;

    // Texture hasn't changed since last time, no need to recompute anything
    if(!in_tex.changed)
      return;
    in_tex.changed = false;

    auto& out_tex = outputs.image.texture;

    const int N = in_tex.width * in_tex.height * 4;
    for(int i = 0; i < N; i++)
      out_tex.bytes[i] = in_tex.bytes[(i * 13 + k * 4) % N];

    // weeeeeee
    k++;

    // Call this when the texture changed
    outputs.image.texture.update(bytes.data(), 500, 500);
  }
};
}
