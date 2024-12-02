#include "Library.hpp"

namespace Wokwi
{
void DeviceLibrary::initMCUs()
{
  auto gpio = [](uint8_t i) {
    return MCUCapability{
        .type = CapabilityType::GPIO, .line = i, .input = true, .output = true};
  };
  auto adc = [](uint8_t dev, uint8_t chan) {
    return MCUCapability{
        .type = CapabilityType::Analog,
        .device = dev,
        .channel = chan,
        .input = true,
        .output = false};
  };
  auto dac = [](uint8_t dev, uint8_t chan) {
    return MCUCapability{
        .type = CapabilityType::Analog,
        .device = dev,
        .channel = chan,
        .input = false,
        .output = true};
  };

  /// Init MCUs ///
  /// RP2040 ///
  auto pwm_a = [](uint8_t dev) {
    return MCUCapability{
        .type = CapabilityType::PWM,
        .device = dev,
        .channel = 0,
        .input = false,
        .output = true};
  };
  auto pwm_b = [](uint8_t dev) {
    return MCUCapability{
        .type = CapabilityType::PWM,
        .device = dev,
        .channel = 1,
        .input = true,
        .output = true};
  };

  mcus.push_back(
      MCU{.name = "rp2040",
          .pins = {
              {"GPIO0", {gpio(0), pwm_a(0)}},
              {"GPIO1", {gpio(1), pwm_b(0)}},
              {"GPIO2", {gpio(2), pwm_a(1)}},
              {"GPIO3", {gpio(3), pwm_b(1)}},
              {"GPIO4", {gpio(4), pwm_a(2)}},
              {"GPIO5", {gpio(5), pwm_b(2)}},
              {"GPIO6", {gpio(6), pwm_a(3)}},
              {"GPIO7", {gpio(7), pwm_b(3)}},
              {"GPIO8", {gpio(8), pwm_a(4)}},
              {"GPIO9", {gpio(9), pwm_b(4)}},
              {"GPIO10", {gpio(10), pwm_a(5)}},
              {"GPIO11", {gpio(11), pwm_b(5)}},
              {"GPIO12", {gpio(12), pwm_a(6)}},
              {"GPIO13", {gpio(13), pwm_b(6)}},
              {"GPIO14", {gpio(14), pwm_a(7)}},
              {"GPIO15", {gpio(15), pwm_b(7)}},
              {"GPIO16", {gpio(16), pwm_a(0)}},
              {"GPIO17", {gpio(17), pwm_b(0)}},
              {"GPIO18", {gpio(18), pwm_a(1)}},
              {"GPIO19", {gpio(19), pwm_b(1)}},
              {"GPIO20", {gpio(20), pwm_a(2)}},
              {"GPIO21", {gpio(21), pwm_b(2)}},
              {"GPIO22", {gpio(22), pwm_a(3)}},
              {"GPIO23", {gpio(23), pwm_b(3)}},
              {"GPIO24", {gpio(24), pwm_a(4)}},
              {"GPIO25", {gpio(25), pwm_b(4)}},
              {"GPIO26", {gpio(26), pwm_a(5), adc(0, 0)}},
              {"GPIO27", {gpio(27), pwm_b(5), adc(0, 1)}},
              {"GPIO28", {gpio(28), pwm_a(6), adc(0, 2)}},
              {"GPIO29", {gpio(29), pwm_b(6), adc(0, 3)}},
          }});

  /// ESP32 ///
  auto pwm = []() {
    return MCUCapability{
        .type = CapabilityType::PWM,
        .device = 0,
        .channel = 0,
        .input = false,
        .output = true};
  };
  mcus.push_back(
      MCU{.name = "esp32",
          .pins = {
              {"GPIO0", {gpio(0), pwm(), adc(2, 1)}},
              {"GPIO1", {gpio(1), pwm()}},
              {"GPIO2", {gpio(2), pwm(), adc(2, 2)}},
              {"GPIO3", {gpio(3), pwm()}},
              {"GPIO4", {gpio(4), pwm(), adc(2, 0)}},
              {"GPIO5", {gpio(5), pwm()}},
              {"GPIO6", {gpio(6), pwm()}},
              {"GPIO7", {gpio(7), pwm()}},
              {"GPIO8", {gpio(8), pwm()}},
              {"GPIO9", {gpio(9), pwm()}},
              {"GPIO10", {gpio(10), pwm()}},
              {"GPIO11", {gpio(11), pwm()}},
              {"GPIO12", {gpio(12), pwm(), adc(2, 5)}},
              {"GPIO13", {gpio(13), pwm(), adc(2, 4)}},
              {"GPIO14", {gpio(14), pwm(), adc(2, 6)}},
              {"GPIO15", {gpio(15), pwm(), adc(2, 3)}},
              {"GPIO16", {gpio(16), pwm()}},
              {"GPIO17", {gpio(17), pwm()}},
              {"GPIO18", {gpio(18), pwm()}},
              {"GPIO19", {gpio(19), pwm()}},
              {"GPIO20", {gpio(20), pwm()}},
              {"GPIO21", {gpio(21), pwm()}},
              {"GPIO22", {gpio(22), pwm()}},
              {"GPIO23", {gpio(23), pwm()}},
              {"GPIO24", {gpio(24), pwm()}},
              {"GPIO25", {gpio(25), pwm(), adc(2, 8), dac(0, 1)}},
              {"GPIO26", {gpio(26), pwm(), adc(2, 9), dac(0, 2)}},
              {"GPIO27", {gpio(27), pwm(), adc(2, 7)}},
              {"GPIO28", {gpio(28), pwm()}},
              {"GPIO29", {gpio(29), pwm()}},
              {"GPIO30", {gpio(30), pwm()}},
              {"GPIO31", {gpio(31), pwm()}},
              {"GPIO32", {gpio(32), pwm(), adc(1, 4)}},
              {"GPIO33", {gpio(33), pwm(), adc(1, 5)}},
              {"GPIO34", {gpio(34), adc(1, 6)}}, // No PWM
              {"GPIO35", {gpio(35), adc(1, 7)}}, // No PWM
              {"GPIO36", {gpio(36), adc(1, 0)}}, // No PWM
              {"GPIO37", {gpio(37), pwm()}},
              {"GPIO38", {gpio(38), pwm()}},
              {"GPIO39", {gpio(39), adc(1, 3)}}, // No PWM
          }});
  // FIXME ESP32-S3, etc.
}
}
