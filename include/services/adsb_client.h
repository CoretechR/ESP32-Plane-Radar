#pragma once

#include <cstddef>
#include <cstdint>

namespace services::adsb {

struct Aircraft {
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  uint32_t last_update_ms;
  char callsign[9];
  char type[5];
  char alt[12];
};

constexpr size_t kMaxAircraft = 64;

size_t aircraftCount();
const Aircraft* aircraftList();
void deadReckonPosition(const Aircraft& aircraft, uint32_t now_ms,
                        float* out_lat, float* out_lon);

/** Hook invoked during long HTTP I/O (e.g. wifiLoop). Optional. */
using PollFn = void (*)();
void setPollFn(PollFn fn);

/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

}  // namespace services::adsb
