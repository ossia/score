import ossia_python as ossia
import os, sys, time

### LOCAL DEVICE SETUP
local_device = ossia.LocalDevice("PythonConsole")
local_device.create_oscquery_server(3456, 5678, False)

# PARAMETER : display text in python console
text_parameter = local_device.add_node("/text").create_parameter(ossia.ValueType.String)
text_parameter.access_mode = ossia.AccessMode.Set

def text_callback(v):
  print(v)
text_parameter.add_callback(text_callback)

# PARAMETER : display a prompt text in python console and wait for input
prompt_parameter = local_device.add_node("/prompt").create_parameter(ossia.ValueType.String)
prompt_parameter.access_mode = ossia.AccessMode.Set

# PARAMETER : input comming from a prompt
prompt_input_parameter = local_device.add_node("/prompt/input").create_parameter(ossia.ValueType.String)
prompt_input_parameter.access_mode = ossia.AccessMode.Get

def prompt_callback(v):
	if sys.version_info[0] > 2:
  		prompt_input_parameter.value = input(v + "\n")
	else:
  		prompt_input_parameter.value = raw_input(v + "\n")
prompt_parameter.add_callback(prompt_callback)

# PARAMETER : clear python console
clear_parameter = local_device.add_node("/clear").create_parameter(ossia.ValueType.Impulse)
clear_parameter.access_mode = ossia.AccessMode.Set

def clear_callback(v):
  os.system('clear')
clear_parameter.add_callback(clear_callback)

# PARAMETER : clear python console
exit_parameter = local_device.add_node("/exit").create_parameter(ossia.ValueType.Bool)
exit_parameter.access_mode = ossia.AccessMode.Set

### MAIN LOOP
print("\nPYTHON CONSOLE\n")
while exit_parameter.value != True:
  time.sleep(0.01)

print("\nEXIT PYTHON CONSOLE\n")
