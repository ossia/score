#pragma once

/*! \page GfxPlugins Graphics plug-ins
 *
 * This section covers creating custom visual effect processes and nodes.
 * See score::gfx and Gfx namespace documentation beforehand.
 *
 * \section CustomNode Creating a custom node
 *
 * To create a custom node, one must inherit from score::gfx::Node and score::gfx::NodeRenderer.
 * A good example is score::gfx::TexgenNode which generates a texture on the CPU from a C++ function.
 *
 * The process is as follows :
 *
 * - Define the material of the node in a struct:
 *
 * ```
 *  ⠀
 * ```
 *
 * ```
 * #pragma pack(push, 1)
 * struct MyShaderParameters {
 *    int foo;
 *    float bar[2];
 * } my_params;
 * #pragma pack(pop)
 * ```
 *
 * The struct must respect GLSL std140 rules; sometimes additional explicit padding is needed
 * even with pragma pack.
 *
 * - Add matching ports in the constructor:
 *
 * ```cpp
 * input.push_back(new Port{this, &my_params.foo, Types::Int, {}});
 * input.push_back(new Port{this, my_params.bar, Types::Vec2, {}});
 * input.push_back(new Port{this, {}, Types::Image, {}});
 * output.push_back(new Port{this, {}, Types::Image, {}});
 * ```
 *
 * - Write the shaders for your node (see ShaderConventions).
 *
 * For instance, for the above definition, the material_t UBO would look like this:
 *
 * ```glsl
 * layout(std140, binding = 2) uniform material_t {
 *   int foo;
 *   vec2 bar;
 * }
 * ```
 *
 * A sampler would also be created:
 *
 * ```glsl
 * layout(binding = 3) uniform sampler2D my_input_texture;
 * ```
 *
 * - Create a renderer class which inherits from score::gfx::GenericNodeRenderer
 *   (to use the score conventions which simplify the most common things) or score::gfx::NodeRenderer
 *   (to have a complete control on resources, pipelines, etc).
 *
 * See score::gfx::ImagesNode and score::gfx::TexgenNode for simple examples using
 * the "pre-made" classes, and score::gfx::ISFNode or score::gfx::VideoNode for more complex
 * example which do implement everything.
 *
 * Once that is done, the only remaining thing is to create a score process which will allow
 * the node to be used in the score.
 *
 * \section ShaderConventions Shader conventions
 *
 * If one inherits from the base classes score::gfx::Node and score::gfx::NodeRenderer,
 * there is no restriction: shaders can have whatever UBOs, samplers, attributes, ...
 * that the node author wishes. Of course the mesh used must be compatible with the shader attributes.
 *
 * On the other hand, one may opt-in to the conventions of score, which allows to use the
 * simplified score::gfx::buildPipeline and score::gfx::DefaultShaderMaterial utilities.
 *
 * In that case, vertex shaders shall follow this template:
 *
 * ```glsl
 * #version 450
 *
 * // Mesh input. Most common case is a full-screen quad / triangle which is
 * // reflected by the 2d positions used.
 * // See PhongNode for a real 3D example.
 * layout(location = 0) in vec2 position;
 * layout(location = 1) in vec2 texcoord;
 *
 * layout(location = 0) out vec2 v_texcoord;
 *
 * // Always present.
 * // Corresponds to the renderer global settings:
 * // - the final output size
 * // - adjustment factors which depend on the graphics API used.
 * layout(std140, binding = 0) uniform renderer_t {
 *   mat4 clipSpaceCorrMatrix;
 *   vec2 texcoordAdjust;
 *   vec2 renderSize;
 * };
 *
 * // Always present.
 * // Corresponds to the score process execution information.
 * // Mostly taken from the ISF and ShaderToy specs.
 * layout(std140, binding = 1) uniform process_t {
 *   float time;
 *   float timeDelta;
 *   float progress;
 *
 *   int passIndex;
 *   int frameIndex;
 *
 *   vec4 date;
 *   vec4 mouse;
 *   vec4 channelTime;
 *
 *  float sampleRate;
 * };
 *
 * // This UBO is generated from the input ports of the score::gfx::Node
 * // when using a DefaultShaderMaterial.
 * layout(std140, binding = 2) uniform material_t {
 * }
 *
 * // Samplers corresponding to input image ports
 * // are automatically added if using a DefaultShaderMaterial, starting from binding = 3
 * layout(binding = 3) uniform sampler2D first_sampler;
 * layout(binding = 4) uniform sampler2D second_sampler;
 *
 * out gl_PerVertex { vec4 gl_Position; };
 *
 * void main()
 * {
 *   v_texcoord = texcoord;
 *   gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
 * }
 * ```
 *
 * Fragment shaders shall follow this template:
 *
 *
 * ```glsl
 * #version 450
 *
 * layout(location = 0) in vec2 v_texcoord;
 *
 * // <!> insert here all the UBOs used in the vertex shader <!>
 * // NOTE : some APIs require the *exact* same variable names & definitions to be used in
 * // all the pipeline stages so be careful with it.
 *
 * layout(location = 0) out vec4 fragColor;
 *
 *
 * void main ()
 * {
 *   fragColor = ...;
 * }
 * ```
 *
 * \section VideoDecoders Video decoders
 *
 * If one wants to add support for GPU decoding of some video frame data, the
 * it is sufficient to inherit from score::gfx::GPUVideoDecoder.
 *
 * The available decoders are then listed in VideoNodeRenderer::createGpuDecoder() ;
 * the AVFormat / fourcc is used to create the correct decoder for the input format.
 *
 * See for instance score::gfx::YUV420Decoder for an example of GPU decoding of tri-planar YUV420 video.
 */
