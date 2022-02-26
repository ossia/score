#pragma once
#include <Crousti/Attributes.hpp>
#include <ossia/detail/logger.hpp>

namespace examples
{
/**
 * This example is the simplest possible score process. It will just print a hello world
 * message repeatedly when executed.
 */
struct EmptyExample
{
  /** Here are the metadata of the plug-ins, to display to the user **/
  meta_attribute(pretty_name, "Hello world");
  meta_attribute(script_name, empty_example);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "1c0f91ee-52da-4a49-a70a-4530a24b152b");

  /** This function will be called repeatedly at the tick rate of the environment **/
  void operator()()
  {
    ossia::logger().info("henlo");
  }
};
}
