#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Concepts.hpp>
#include <rnd/random.hpp>

namespace examples
{
/**
 * The next part is about working with the video system.
 *
 * Note that this is a very inefficient way of doing generation / processing
 * of video data, as this is done on the CPU ; this should
 * only ever be used if the algorithm is not implementable as a GPU shader,
 * with the ISF format that score supports.
 * For instance, if you want to use some OpenCV or Dlib goodness.
 *
 * Note that the API is a bit more limited, as in this case the nodes
 * run in the GPU processing thread, at the screen / output's frame rate, instead
 * of running in the audio thread, at audio buffer rate.
 *
 * In particular, currently, timing information is not accessible, and it is only
 * possible to have an output texture.
 */
struct TextureGeneratorExample
{
  meta_attribute(pretty_name, "My example texture generator");
  meta_attribute(script_name, texture_gen);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "01247f4f-6b19-458d-845d-9f7cc2d9d663");

  // By know you know the drill: define inputs, outputs...
  struct {
    struct {
      // FIXME meta_control(Control::LogFloatSlider, "Bamboozling", 0.0001f, 0.1f, 0.f);

      float value = 0.f;
    } bamboozle;
  } inputs;

  struct {
    struct {
      meta_attribute(name, "Out");

      // This type is a view on a texture
      oscr::rgba_texture texture;
    } image;
  } outputs;

  // Some place in RAM to store our pixels
  boost::container::vector<unsigned char> bytes;

  TextureGeneratorExample()
  {
    // Allocate some initial data
    bytes = oscr::rgba_texture::allocate(480, 270);
    for(unsigned char& c : bytes)
    {
      c = rnd::rand(0, 10);
    }
  }

  // Note that as soon as we use textures,
  // we run at frame rate (e.g. 60hz) instead of audio buffer rate
  // (e.g. maybe 1000hz if you have a decent enough soundcard).
  void operator()()
  {
    // Do some magic
    int k = 0;
    for (unsigned char& c : bytes)
    {
      c += k++ * inputs.bamboozle.value;
    }

    // Call this when the texture changed
    outputs.image.texture.update(bytes.data(), 480, 270);
  }
};
}
