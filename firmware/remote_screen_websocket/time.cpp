#include <Arduino.h>
#include "time.h"

uint64_t get_uptime_ms() {
  /* http://arduino.palvelum.me/?p=177 */
  static uint32_t low32, high32;
  uint32_t new_low32 = millis();
  if (new_low32 < low32) high32++;
  low32 = new_low32;
  return (uint64_t) high32 << 32 | low32;
}