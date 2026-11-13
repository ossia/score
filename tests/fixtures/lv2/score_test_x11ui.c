/* The X11 UI of urn:score:test:x11ui, for the tests of the window score
 * embeds an LV2 UI in.
 *
 * It makes a 300 x 200 window in the parent the host gives (ui:parent), on a
 * display connection of its own as pugl-based UIs (DPF, BWidgets) do, and
 * resizes it itself when its "size" control goes over 0.5: to 400 x 250, as
 * DPF's resize handle does, without asking the host.
 */
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>

#include <X11/Xlib.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  Display* display;
  Window window;
} test_x11ui;

static LV2UI_Handle instantiate(
    const LV2UI_Descriptor* descriptor, const char* plugin_uri,
    const char* bundle_path, LV2UI_Write_Function write_function,
    LV2UI_Controller controller, LV2UI_Widget* widget,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)plugin_uri;
  (void)bundle_path;
  (void)write_function;
  (void)controller;

  Window parent = 0;
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_UI__parent))
      parent = (Window)(uintptr_t)features[i]->data;
  if(!parent)
    return NULL;

  test_x11ui* self = (test_x11ui*)calloc(1, sizeof(test_x11ui));
  self->display = XOpenDisplay(NULL);
  if(!self->display)
  {
    free(self);
    return NULL;
  }
  self->window = XCreateSimpleWindow(
      self->display, parent, 0, 0, 300, 200, 0, 0, 0x336699);
  XMapWindow(self->display, self->window);
  XSync(self->display, False);
  *widget = (LV2UI_Widget)(uintptr_t)self->window;
  return self;
}

static void cleanup(LV2UI_Handle handle)
{
  test_x11ui* self = (test_x11ui*)handle;
  XDestroyWindow(self->display, self->window);
  XCloseDisplay(self->display);
  free(self);
}

static void port_event(
    LV2UI_Handle handle, uint32_t port, uint32_t size, uint32_t format,
    const void* buffer)
{
  test_x11ui* self = (test_x11ui*)handle;
  if(port != 0 || format != 0 || size != sizeof(float))
    return;
  const float v = *(const float*)buffer;
  if(v > 0.5f)
    XResizeWindow(self->display, self->window, 400, 250);
  else
    XResizeWindow(self->display, self->window, 300, 200);
  XSync(self->display, False);
}

static const void* extension_data(const char* uri)
{
  (void)uri;
  return NULL;
}

static const LV2UI_Descriptor descriptor
    = {"urn:score:test:x11ui#ui", instantiate, cleanup, port_event, extension_data};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
  return index == 0 ? &descriptor : NULL;
}
