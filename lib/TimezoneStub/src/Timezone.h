// Minimal stand-in for the Timezone library's types (original: JChristensen/Timezone,
// GPL-3.0, which this firmware must not link). The vendored MQTT bridge only stores
// rules in a Timezone object; it never converts times with it (timestamps are UTC),
// so no conversion functions are provided. Written from the API as used, MIT licensed.
#pragma once

#include <stdint.h>
#include <string.h>
#include <time.h>

enum week_t { Last, First, Second, Third, Fourth };
enum dow_t { Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat };
enum month_t { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

struct TimeChangeRule {
  char abbrev[6];
  uint8_t week;    // week_t
  uint8_t dow;     // dow_t
  uint8_t month;   // month_t
  uint8_t hour;
  int offset;      // minutes from UTC
};

class Timezone {
public:
  Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart) : _dst(dstStart), _std(stdStart) {}
  void setRules(TimeChangeRule dstStart, TimeChangeRule stdStart) { _dst = dstStart; _std = stdStart; }

private:
  TimeChangeRule _dst, _std;
};
