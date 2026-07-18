#include "ui/radar_display.h"

#include <lgfx/v1/lgfx_fonts.hpp>
#include <pgmspace.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"
#include "ui/runway_overlay.h"

//namespace fonts = lgfx::v1::fonts;

namespace ui {
namespace radar {

uint16_t kColorBackground = 0x0000;
uint16_t kColorGrid = 0x0320;
uint16_t kColorLabel = 0xFFFF;
uint16_t kColorCenter = 0xFFFF;
uint16_t kColorAircraft = 0x001F;
uint16_t kColorTrackVector = 0xFFFF;
uint16_t kColorTagType = 0x5DFF;
uint16_t kColorTagAltitude = 0xFFE0;
uint16_t kColorRunway = 0x4D5F;
uint16_t kColorRunwayLabel = 0x7DFF;

}  // namespace radar

namespace {

bool s_label_metrics_ready = false;
bool s_cardinal_use_vlw = false;
bool s_scale_use_vlw = false;
float s_cardinal_vlw_size = 0.56f;
float s_scale_vlw_size = 0.50f;
float s_tag_vlw_size = 0.56f;
const lgfx::GFXfont* s_cardinal_gfx = &fonts::FreeSansBold12pt7b;
const lgfx::GFXfont* s_scale_gfx = &fonts::FreeSansBold9pt7b;
const lgfx::GFXfont* s_tag_gfx = &fonts::FreeSansBold12pt7b;

bool s_tag_label_metrics_ready = false;
bool s_tag_use_vlw = false;

int s_scale_label_max_w = 0;
int s_scale_label_h = 0;

lgfx::LovyanGFX* s_draw = &tft;
LGFX_Sprite s_frame(&tft);
bool s_frame_ready = false;

constexpr int kAirlinerSpriteSize = 15;
constexpr int kAirlinerSpriteHalf = kAirlinerSpriteSize / 2;
const uint8_t kSpriteNarrowbody[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 235, 237, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 60, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {64, 0, 0, 0, 0, 0, 0, 255, 255, 255, 144, 0, 0, 0, 0},
    {255, 255, 0, 0, 0, 0, 0, 255, 255, 255, 184, 0, 0, 0, 0},
    {159, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {128, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 0, 0, 0, 0, 0, 255, 255, 255, 196, 0, 0, 0, 0},
    {96, 0, 0, 0, 0, 0, 0, 255, 255, 255, 144, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 239, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 60, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 243, 237, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

const uint8_t kSpriteWidebody[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 255, 240, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 18, 255, 140, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 199, 255, 141, 0, 0, 0, 0, 0, 0},
    {9, 69, 0, 0, 0, 0, 137, 255, 255, 255, 171, 0, 0, 0, 0},
    {77, 255, 30, 0, 0, 0, 152, 255, 255, 255, 255, 0, 0, 0, 0},
    {0, 255, 255, 189, 255, 255, 255, 255, 255, 255, 255, 255, 255, 148, 0},
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {0, 255, 255, 176, 255, 255, 255, 255, 255, 255, 255, 255, 255, 177, 7},
    {70, 255, 27, 0, 0, 0, 166, 255, 255, 255, 255, 0, 0, 0, 0},
    {15, 95, 0, 0, 0, 0, 138, 255, 255, 255, 203, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 215, 255, 146, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 253, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 20, 255, 144, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 252, 240, 0, 0, 0, 0, 0, 0, 0, 0},
};

const uint8_t kSpriteRegionalJet[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 235, 237, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 60, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {64, 0, 0, 0, 0, 0, 0, 255, 255, 255, 144, 0, 0, 0, 0},
    {255, 255, 0, 0, 0, 0, 0, 255, 255, 255, 184, 0, 0, 0, 0},
    {159, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {128, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 0, 0, 0, 0, 0, 255, 255, 255, 196, 0, 0, 0, 0},
    {96, 0, 0, 0, 0, 0, 0, 255, 255, 255, 144, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 239, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 60, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 243, 237, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

const uint8_t kSpriteTurboprop[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 255, 88, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 8, 0, 0, 0, 0, 4, 255, 255, 47, 0, 0, 0, 0, 0},
    {0, 255, 255, 0, 0, 0, 255, 255, 255, 255, 17, 0, 0, 0, 0},
    {0, 255, 255, 156, 255, 255, 255, 255, 255, 255, 255, 255, 255, 3, 0},
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0},
    {0, 255, 255, 197, 255, 255, 255, 255, 255, 255, 255, 255, 255, 17, 0},
    {0, 255, 255, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0},
    {0, 20, 5, 0, 0, 0, 5, 255, 255, 39, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 69, 0, 0, 0, 0, 0, 0},
};

const uint8_t kSpriteBusinessJet[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 24, 0, 0, 0, 0, 0, 0},
    {255, 255, 0, 0, 34, 7, 255, 255, 255, 0, 0, 0, 0, 0, 0},
    {255, 255, 12, 255, 255, 255, 255, 255, 255, 226, 226, 226, 226, 85, 0},
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 12, 255, 255, 255, 255, 255, 255, 236, 236, 236, 236, 91, 0},
    {255, 249, 0, 0, 45, 16, 255, 255, 255, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 32, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 12, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

  const uint8_t kSpriteSingleEngine[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 69, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 154, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 62, 255, 255, 176, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 186, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 196, 0, 0, 0, 0},
    {0, 0, 209, 255, 78, 0, 0, 255, 255, 255, 186, 21, 0, 0, 0},
    {0, 0, 255, 255, 154, 48, 142, 255, 255, 255, 246, 245, 128, 0, 0},
    {0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0},
    {0, 0, 255, 255, 194, 152, 213, 255, 255, 255, 254, 252, 128, 0, 0},
    {0, 0, 226, 255, 76, 0, 0, 255, 255, 255, 186, 45, 10, 0, 0},
    {0, 0, 0, 14, 0, 0, 0, 255, 255, 255, 196, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 186, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 128, 255, 255, 176, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 154, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 102, 0, 0, 0, 0},
};

const uint8_t kSpriteHelicopter[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 54, 172, 0, 0, 0, 0, 0, 0, 0, 229, 128},
    {0, 0, 0, 0, 150, 255, 168, 0, 0, 0, 0, 0, 239, 255, 53},
    {0, 0, 0, 0, 0, 225, 255, 199, 0, 0, 0, 203, 255, 118, 0},
    {0, 0, 255, 255, 163, 0, 187, 255, 210, 0, 179, 255, 128, 0, 0},
    {0, 60, 64, 255, 102, 60, 188, 255, 255, 255, 255, 255, 255, 201, 0},
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0},
    {0, 49, 64, 255, 100, 60, 210, 255, 255, 255, 255, 255, 255, 219, 0},
    {0, 0, 234, 255, 169, 0, 26, 255, 234, 0, 60, 255, 255, 0, 0},
    {0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 40, 255, 255, 0},
    {0, 0, 0, 0, 5, 255, 254, 0, 0, 0, 0, 0, 46, 255, 224},
    {0, 0, 0, 0, 30, 231, 0, 0, 0, 0, 0, 0, 0, 56, 153},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

bool startsWith(const char* value, const char* prefix) {
  if (value == nullptr || prefix == nullptr) {
    return false;
  }
  const size_t prefix_len = strlen(prefix);
  if (prefix_len == 0) {
    return false;
  }
  return strncmp(value, prefix, prefix_len) == 0;
}

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool hasAnyDigit(const char* value) {
  if (value == nullptr) {
    return false;
  }
  for (size_t i = 0; value[i] != '\0'; ++i) {
    if (isAsciiDigit(value[i])) {
      return true;
    }
  }
  return false;
}

bool isLikelyLightSingleEngineType(const char* type) {
  if (type == nullptr || type[0] == '\0') {
    return false;
  }

  const size_t len = strlen(type);
  if (len < 2 || len > 4 || !hasAnyDigit(type)) {
    return false;
  }

  // Exclude common airliner/regional/turboprop/business/heli prefixes first.
  if (startsWith(type, "A") || startsWith(type, "B") ||
      startsWith(type, "E") || startsWith(type, "CRJ") ||
      startsWith(type, "DH") || startsWith(type, "AT") ||
      startsWith(type, "SF") || startsWith(type, "MD") ||
      startsWith(type, "SU") || startsWith(type, "EC") ||
      startsWith(type, "AS") || startsWith(type, "AW") ||
      startsWith(type, "R44") || startsWith(type, "R66") ||
      startsWith(type, "C5") || startsWith(type, "C6") ||
      startsWith(type, "C7") || startsWith(type, "LJ") ||
      startsWith(type, "GL") || startsWith(type, "FA") ||
      startsWith(type, "PC12")) {
    return false;
  }

  // Typical GA / ultralight prefixes around Europe.
  return startsWith(type, "C1") || startsWith(type, "C2") ||
         startsWith(type, "C3") || startsWith(type, "C4") ||
         startsWith(type, "PA") || startsWith(type, "P2") ||
         startsWith(type, "P3") || startsWith(type, "SR") ||
         startsWith(type, "S10") || startsWith(type, "S12") ||
         startsWith(type, "VL") || startsWith(type, "WT") ||
         startsWith(type, "FK") || startsWith(type, "CT") ||
         startsWith(type, "UL");
}

const uint8_t (*spriteForType(const char* type))[kAirlinerSpriteSize] {
  if (type == nullptr || type[0] == '\0') {
    return kSpriteNarrowbody;
  }

  // Frankfurt traffic tuned mapping:
  // narrowbody-heavy LH/4U/EW and many European airlines.
  if (startsWith(type, "A31") || startsWith(type, "A19") ||
      startsWith(type, "A20") || startsWith(type, "A21") ||
      startsWith(type, "B73") || startsWith(type, "B38") ||
      startsWith(type, "B39") || startsWith(type, "B75") ||
      startsWith(type, "A22") || startsWith(type, "A32")) {
    return kSpriteNarrowbody;
  }

  // FRA long-haul and cargo heavy types.
  if (startsWith(type, "A33") || startsWith(type, "A34") ||
      startsWith(type, "A35") || startsWith(type, "A38") ||
      startsWith(type, "B74") || startsWith(type, "B77") ||
      startsWith(type, "B78") || startsWith(type, "MD11") ||
      startsWith(type, "B76")) {
    return kSpriteWidebody;
  }

  // Regional jets common in Germany/Europe feeder traffic.
  if (startsWith(type, "E17") || startsWith(type, "E18") ||
      startsWith(type, "E19") || startsWith(type, "E2") ||
      startsWith(type, "CRJ") || startsWith(type, "C55") ||
      startsWith(type, "SU9")) {
    return kSpriteRegionalJet;
  }

  if (startsWith(type, "AT") || startsWith(type, "DH8") ||
      startsWith(type, "DHC") || startsWith(type, "SF3") ||
      startsWith(type, "C27") || startsWith(type, "F50")) {
    return kSpriteTurboprop;
  }

  if (startsWith(type, "EC") || startsWith(type, "AS") ||
      startsWith(type, "AW") || startsWith(type, "R44") ||
      startsWith(type, "R66") || type[0] == 'H') {
    return kSpriteHelicopter;
  }

  // Small piston single-engine / ultralight / sport aircraft.
  if (startsWith(type, "C1") || startsWith(type, "C2") ||
      startsWith(type, "C3") || startsWith(type, "C4") ||
      startsWith(type, "PA1") || startsWith(type, "PA2") ||
      startsWith(type, "PA3") || startsWith(type, "P28") ||
      startsWith(type, "P32") || startsWith(type, "P46") ||
      startsWith(type, "SR2") || startsWith(type, "S10") ||
      startsWith(type, "S12") || startsWith(type, "UL") ||
      startsWith(type, "FK") || startsWith(type, "CT") ||
      startsWith(type, "WT9") || startsWith(type, "VL3") ||
      startsWith(type, "EUPA")) {
    return kSpriteSingleEngine;
  }

  if (isLikelyLightSingleEngineType(type)) {
    return kSpriteSingleEngine;
  }

  if (startsWith(type, "C5") || startsWith(type, "C6") ||
      startsWith(type, "C7") || startsWith(type, "LJ") ||
      startsWith(type, "FA") || startsWith(type, "GL") ||
      startsWith(type, "PC12")) {
    return kSpriteBusinessJet;
  }

  return kSpriteNarrowbody;
}

class DrawScope {
 public:
  explicit DrawScope(lgfx::LovyanGFX& gfx) : prev_(s_draw) { s_draw = &gfx; }
  ~DrawScope() { s_draw = prev_; }

 private:
  lgfx::LovyanGFX* prev_;
};

int absDiff(int a, int b) { return std::abs(a - b); }

uint16_t blendRgb565(uint16_t fg, uint16_t bg, uint8_t fg_alpha) {
  const uint8_t bg_alpha = static_cast<uint8_t>(255 - fg_alpha);

  const uint8_t fr5 = static_cast<uint8_t>((fg >> 11) & 0x1F);
  const uint8_t fg6 = static_cast<uint8_t>((fg >> 5) & 0x3F);
  const uint8_t fb5 = static_cast<uint8_t>(fg & 0x1F);

  const uint8_t br5 = static_cast<uint8_t>((bg >> 11) & 0x1F);
  const uint8_t bg6 = static_cast<uint8_t>((bg >> 5) & 0x3F);
  const uint8_t bb5 = static_cast<uint8_t>(bg & 0x1F);

  const uint8_t out_r5 = static_cast<uint8_t>((fr5 * fg_alpha + br5 * bg_alpha) / 255);
  const uint8_t out_g6 = static_cast<uint8_t>((fg6 * fg_alpha + bg6 * bg_alpha) / 255);
  const uint8_t out_b5 = static_cast<uint8_t>((fb5 * fg_alpha + bb5 * bg_alpha) / 255);

  return static_cast<uint16_t>((out_r5 << 11) | (out_g6 << 5) | out_b5);
}

float fpart(float x) { return x - floorf(x); }

float rfpart(float x) { return 1.0f - fpart(x); }

void plotAaPixel(int x, int y, uint16_t color, float coverage) {
  if (coverage <= 0.0f || x < 0 || y < 0 || x >= radar::kSize || y >= radar::kSize) {
    return;
  }

  if (coverage > 1.0f) {
    coverage = 1.0f;
  }

  const uint8_t alpha = static_cast<uint8_t>(coverage * 255.0f + 0.5f);
  const uint16_t base = s_draw->readPixel(x, y);
  const uint16_t px = blendRgb565(color, base, alpha);
  s_draw->drawPixel(x, y, px);
}

void drawAaLine(int x0, int y0, int x1, int y1, uint16_t color) {
  bool steep = absDiff(y1, y0) > absDiff(x1, x0);
  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }

  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float gradient = (dx == 0.0f) ? 0.0f : (dy / dx);

  const float xend1 = roundf(static_cast<float>(x0));
  const float yend1 = static_cast<float>(y0) + gradient * (xend1 - static_cast<float>(x0));
  const float xgap1 = rfpart(static_cast<float>(x0) + 0.5f);
  const int xpxl1 = static_cast<int>(xend1);
  const int ypxl1 = static_cast<int>(floorf(yend1));

  if (steep) {
    plotAaPixel(ypxl1, xpxl1, color, rfpart(yend1) * xgap1);
    plotAaPixel(ypxl1 + 1, xpxl1, color, fpart(yend1) * xgap1);
  } else {
    plotAaPixel(xpxl1, ypxl1, color, rfpart(yend1) * xgap1);
    plotAaPixel(xpxl1, ypxl1 + 1, color, fpart(yend1) * xgap1);
  }

  float intery = yend1 + gradient;

  const float xend2 = roundf(static_cast<float>(x1));
  const float yend2 = static_cast<float>(y1) + gradient * (xend2 - static_cast<float>(x1));
  const float xgap2 = fpart(static_cast<float>(x1) + 0.5f);
  const int xpxl2 = static_cast<int>(xend2);
  const int ypxl2 = static_cast<int>(floorf(yend2));

  if (steep) {
    plotAaPixel(ypxl2, xpxl2, color, rfpart(yend2) * xgap2);
    plotAaPixel(ypxl2 + 1, xpxl2, color, fpart(yend2) * xgap2);
  } else {
    plotAaPixel(xpxl2, ypxl2, color, rfpart(yend2) * xgap2);
    plotAaPixel(xpxl2, ypxl2 + 1, color, fpart(yend2) * xgap2);
  }

  if (xpxl2 - xpxl1 <= 1) {
    return;
  }

  for (int x = xpxl1 + 1; x < xpxl2; ++x) {
    const int y = static_cast<int>(floorf(intery));
    if (steep) {
      plotAaPixel(y, x, color, rfpart(intery));
      plotAaPixel(y + 1, x, color, fpart(intery));
    } else {
      plotAaPixel(x, y, color, rfpart(intery));
      plotAaPixel(x, y + 1, color, fpart(intery));
    }
    intery += gradient;
  }
}

int measureGfxHeight(const lgfx::GFXfont& font) {
  tft.setFont(&font);
  tft.setTextSize(1);
  return tft.fontHeight();
}

int measureVlwHeight(float size) {
  tft.setTextSize(size);
  return tft.fontHeight();
}

float findVlwSizeForHeight(int target_px) {
  float lo = 0.25f;
  float hi = 1.2f;
  for (int i = 0; i < 16; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (measureVlwHeight(mid) < target_px) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return hi;
}

void applyScaleStyle();

const lgfx::GFXfont* pickGfxFontClosest(
    int target_px, const lgfx::GFXfont* const* candidates, size_t count) {
  const lgfx::GFXfont* best = candidates[0];
  int best_diff = absDiff(measureGfxHeight(*best), target_px);

  for (size_t i = 1; i < count; ++i) {
    const int diff = absDiff(measureGfxHeight(*candidates[i]), target_px);
    if (diff < best_diff) {
      best_diff = diff;
      best = candidates[i];
    }
  }
  return best;
}

void initLabelMetrics() {
  if (s_label_metrics_ready) {
    return;
  }

  const int cardinal_target = radar::kCardinalLabelHeightPx;

  if (displayFontIsSmooth()) {
    s_cardinal_use_vlw = true;
    s_cardinal_vlw_size = findVlwSizeForHeight(cardinal_target);
    const int cardinal_h = measureVlwHeight(s_cardinal_vlw_size);
    const int scale_target = cardinal_h - radar::kScaleBelowCardinalPx;
    s_scale_use_vlw = true;
    s_scale_vlw_size = findVlwSizeForHeight(scale_target);
  } else {
    const lgfx::GFXfont* cardinal_candidates[] = {&fonts::FreeSansBold12pt7b,
                                                  &fonts::FreeSansBold9pt7b};
    s_cardinal_gfx =
        pickGfxFontClosest(cardinal_target, cardinal_candidates, 2);
    s_cardinal_use_vlw = false;

    const int cardinal_h = measureGfxHeight(*s_cardinal_gfx);
    const int scale_target = cardinal_h - radar::kScaleBelowCardinalPx;
    const lgfx::GFXfont* scale_candidates[] = {&fonts::FreeSansBold9pt7b,
                                               &fonts::FreeSansBold12pt7b};
    s_scale_gfx = pickGfxFontClosest(scale_target, scale_candidates, 2);
    s_scale_use_vlw = false;
  }

  applyScaleStyle();
  s_scale_label_h = tft.fontHeight();
  s_scale_label_max_w = 0;
  char label[12];
  for (size_t i = 0; i < radar::kRangePresetCount; ++i) {
    for (bool miles : {false, true}) {
      radar::formatRing3Label(label, sizeof(label), radar::kRangePresets[i].ring3_km,
                              miles);
      const int w = tft.textWidth(label);
      if (w > s_scale_label_max_w) {
        s_scale_label_max_w = w;
      }
    }
  }

  s_label_metrics_ready = true;
}

void initTagLabelMetrics() {
  if (s_tag_label_metrics_ready) {
    return;
  }

  const int target = radar::kAircraftTagLabelHeightPx;
  if (displayFontIsSmooth()) {
    s_tag_use_vlw = true;
    s_tag_vlw_size = findVlwSizeForHeight(target);
  } else {
    const lgfx::GFXfont* tag_candidates[] = {&fonts::FreeSansBold12pt7b,
                                               &fonts::FreeSansBold9pt7b};
    s_tag_gfx = pickGfxFontClosest(target, tag_candidates, 2);
    s_tag_use_vlw = false;
  }

  s_tag_label_metrics_ready = true;
}

void initPalette() {
  radar::kColorBackground = tft.color565(radar::kBgR, radar::kBgG, radar::kBgB);
  radar::kColorGrid = tft.color565(radar::kGridR, radar::kGridG, radar::kGridB);
  radar::kColorLabel = tft.color565(255, 255, 255);
  radar::kColorCenter = tft.color565(255, 255, 255);
  // GC9A01 BGR panel: swap R/B in color565 so logical red renders red on screen.
  if (config::kDisplayRgbOrder) {
    radar::kColorAircraft =
        tft.color565(radar::kAircraftB, radar::kAircraftG, radar::kAircraftR);
  } else {
    radar::kColorAircraft =
        tft.color565(radar::kAircraftR, radar::kAircraftG, radar::kAircraftB);
  }
  radar::kColorTrackVector =
      tft.color565(radar::kTrackR, radar::kTrackG, radar::kTrackB);
  radar::kColorTagType =
      tft.color565(radar::kTagTypeR, radar::kTagTypeG, radar::kTagTypeB);
  radar::kColorTagAltitude =
      tft.color565(radar::kTagAltR, radar::kTagAltG, radar::kTagAltB);
  radar::kColorRunway =
      tft.color565(radar::kRunwayR, radar::kRunwayG, radar::kRunwayB);
  radar::kColorRunwayLabel = tft.color565(radar::kRunwayLabelR, radar::kRunwayLabelG,
                                          radar::kRunwayLabelB);
}

constexpr float kKmPerDeg = 111.0f;

void offsetKmFromCenter(float lat, float lon, float* dx_km, float* dy_km,
                        float* dist_km) {
  *dx_km =
      static_cast<float>(lon - services::location::lon()) * kKmPerDeg;
  *dy_km =
      static_cast<float>(lat - services::location::lat()) * kKmPerDeg;
  *dist_km = sqrtf((*dx_km) * (*dx_km) + (*dy_km) * (*dy_km));
}

float innerRingMaxKm() {
  const float outer_km = radar::rangeCurrent().outer_km;
  return outer_km * (static_cast<float>(radar::kGridOuterRadius -
                                       radar::kAircraftInsideRingInsetPx) /
                     static_cast<float>(radar::kGridOuterRadius));
}

/** Flat lat/lon as x/y: 1° ≈ 111 km, north = screen up. */
void latLonToScreen(float lat, float lon, int* out_x, int* out_y) {
  const float outer_km = radar::rangeCurrent().outer_km;
  const float px_per_km = static_cast<float>(radar::kGridOuterRadius) / outer_km;

  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);

  *out_x = radar::kCenterX + static_cast<int>(lroundf(dx_km * px_per_km));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(dy_km * px_per_km));
}

bool isInsideOuterRingKm(float dist_km) { return dist_km <= innerRingMaxKm(); }

int distSqFromCenter(int x, int y) {
  const int dx = x - radar::kCenterX;
  const int dy = y - radar::kCenterY;
  return dx * dx + dy * dy;
}

bool isInsideOuterRing(int x, int y) {
  const int max_r = radar::kGridOuterRadius - radar::kAircraftInsideRingInsetPx;
  return distSqFromCenter(x, y) <= max_r * max_r;
}

/** Rim dot from true bearing; always on screen edge (even if target is 50+ km away). */
bool beyondRingEdgeDotFromLatLon(float lat, float lon, int* out_x, int* out_y) {
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  if (dist_km < 0.01f) {
    return false;
  }
  if (isInsideOuterRingKm(dist_km)) {
    return false;
  }

  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int rim_r = radar::kCenterX - radar::kBeyondRingScreenMarginPx;
  const float angle_rad = atan2f(dx_km, dy_km);

  *out_x = cx + static_cast<int>(lroundf(sinf(angle_rad) * rim_r));
  *out_y = cy - static_cast<int>(lroundf(cosf(angle_rad) * rim_r));
  return true;
}

void drawBeyondRingDot(int x, int y) {
  s_draw->fillSmoothCircle(x, y, radar::kBeyondRingDotRadiusPx,
                           radar::kColorAircraft);
}

void clipPointToOuterRing(int x0, int y0, int* x1, int* y1) {
  const int max_r = radar::kGridOuterRadius;
  const int max_r_sq = max_r * max_r;
  if (distSqFromCenter(*x1, *y1) <= max_r_sq) {
    return;
  }

  const int dx = *x1 - x0;
  const int dy = *y1 - y0;
  float t = 1.0f;
  for (int step = 0; step < 20; ++step) {
    const int px = x0 + static_cast<int>(lroundf(dx * t));
    const int py = y0 + static_cast<int>(lroundf(dy * t));
    if (distSqFromCenter(px, py) <= max_r_sq) {
      *x1 = px;
      *y1 = py;
      return;
    }
    t -= 0.05f;
    if (t <= 0.0f) {
      *x1 = x0;
      *y1 = y0;
      return;
    }
  }
}

int speedLineLengthPx(float gs_knots) {
  if (gs_knots <= 0.0f) {
    return 0;
  }

  // Fixed screen scale: 60 s horizon at gs, not tied to current range zoom.
  constexpr float kKmPerKnotPerHorizon =
      1.852f * radar::kAircraftTrackHorizonSec / 3600.0f;
  const float px =
      gs_knots * kKmPerKnotPerHorizon * radar::kGridOuterRadius /
      radar::kAircraftTrackRefOuterKm * radar::kAircraftTrackLengthScale;

  const int len = static_cast<int>(px + 0.5f);
  if (len < radar::kAircraftSpeedLineMinPx) {
    return radar::kAircraftSpeedLineMinPx;
  }
  return len;
}

void noseTip(int cx, int cy, float heading_deg, int* tip_x, int* tip_y) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  *tip_x = cx + static_cast<int>(lroundf(sinf(rad) * radar::kAircraftNoseLenPx));
  *tip_y = cy - static_cast<int>(lroundf(cosf(rad) * radar::kAircraftNoseLenPx));
}

void drawHeadingTriangle(int cx, int cy, float heading_deg, uint16_t color,
                         const char* type) {
  constexpr float kDegToRad = 0.01745329252f;
  // Sprite points to the right in texture space; radar heading uses 0=up.
  // Convert heading so icon nose aligns with the on-screen heading convention.
  const float rad = (90.0f - heading_deg) * kDegToRad;
  const float sin_h = sinf(rad);
  const float cos_h = cosf(rad);
  const uint8_t (*sprite)[kAirlinerSpriteSize] = spriteForType(type);

  for (int dy = -kAirlinerSpriteHalf; dy <= kAirlinerSpriteHalf; ++dy) {
    for (int dx = -kAirlinerSpriteHalf; dx <= kAirlinerSpriteHalf; ++dx) {
      const float sx_f = static_cast<float>(dx) * cos_h -
                         static_cast<float>(dy) * sin_h;
      const float sy_f = static_cast<float>(dx) * sin_h +
                         static_cast<float>(dy) * cos_h;

      const int sx = static_cast<int>(lroundf(sx_f)) + kAirlinerSpriteHalf;
      const int sy = static_cast<int>(lroundf(sy_f)) + kAirlinerSpriteHalf;
      if (sx < 0 || sy < 0 || sx >= kAirlinerSpriteSize ||
          sy >= kAirlinerSpriteSize) {
        continue;
      }

      const uint8_t alpha = pgm_read_byte(&sprite[sy][sx]);
      if (alpha == 0) {
        continue;
      }

      const int px = cx + dx;
      const int py = cy + dy;
      if (px < 0 || py < 0 || px >= radar::kSize || py >= radar::kSize) {
        continue;
      }

      const uint16_t base = s_draw->readPixel(px, py);
      const uint16_t out = blendRgb565(color, base, alpha);
      s_draw->drawPixel(px, py, out);
    }
  }
}

void drawSpeedVector(int cx, int cy, float heading_deg, float track_deg,
                     float gs_knots, uint16_t color) {
  const int len = speedLineLengthPx(gs_knots);
  if (len <= 0) {
    return;
  }

  int tip_x = 0;
  int tip_y = 0;
  noseTip(cx, cy, heading_deg, &tip_x, &tip_y);

  constexpr float kDegToRad = 0.01745329252f;
  const float rad = track_deg * kDegToRad;
  int ex = tip_x + static_cast<int>(lroundf(sinf(rad) * len));
  int ey = tip_y - static_cast<int>(lroundf(cosf(rad) * len));
  clipPointToOuterRing(tip_x, tip_y, &ex, &ey);
  if (ex == tip_x && ey == tip_y) {
    return;
  }
  s_draw->drawWideLine(tip_x, tip_y, ex, ey, radar::kAircraftTrackLineHalfWidth,
                       color);
}

void applyTagStyle() {
  if (s_tag_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_tag_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_tag_gfx);
  }
}

int measureTagBlockWidth(const services::adsb::Aircraft& plane) {
  applyTagStyle();
  int max_w = 0;
  if (plane.callsign[0] != '\0') {
    const int w = s_draw->textWidth(plane.callsign);
    if (w > max_w) {
      max_w = w;
    }
  }
  if (plane.type[0] != '\0') {
    const int w = s_draw->textWidth(plane.type);
    if (w > max_w) {
      max_w = w;
    }
  }
  if (plane.alt[0] != '\0') {
    const int w = s_draw->textWidth(plane.alt);
    if (w > max_w) {
      max_w = w;
    }
  }
  return max_w;
}

void drawAircraftTag(int x, int y, const services::adsb::Aircraft& plane) {
  initTagLabelMetrics();
  applyTagStyle();

  const int line_h = s_draw->fontHeight();
  const int block_w = measureTagBlockWidth(plane);
  const int block_h = line_h * 3;
  int ly = y - block_h / 2;

  const int symbol_half =
      radar::kAircraftNoseLenPx + radar::kAircraftTailHalfPx;
  // West (left): tag toward center on the right; east (right): tag on the left.
  const bool tag_on_right = x < radar::kCenterX;
  int anchor_x = 0;
  if (tag_on_right) {
    anchor_x = x + symbol_half + radar::kAircraftLabelGapPx;
    anchor_x = std::min(anchor_x, radar::kSize - block_w - 1);
    s_draw->setTextDatum(textdatum_t::top_left);
  } else {
    anchor_x = x - symbol_half - radar::kAircraftLabelGapPx;
    anchor_x = std::max(anchor_x, block_w + 1);
    s_draw->setTextDatum(textdatum_t::top_right);
  }
  ly = std::max(1, std::min(ly, radar::kSize - block_h - 1));

  if (plane.callsign[0] != '\0') {
    s_draw->setTextColor(radar::kColorLabel, radar::kColorBackground);
    s_draw->drawString(plane.callsign, anchor_x, ly);
  }
  ly += line_h;

  if (plane.type[0] != '\0') {
    s_draw->setTextColor(radar::kColorTagType, radar::kColorBackground);
    s_draw->drawString(plane.type, anchor_x, ly);
  }
  ly += line_h;

  if (plane.alt[0] != '\0') {
    s_draw->setTextColor(radar::kColorTagAltitude, radar::kColorBackground);
    s_draw->drawString(plane.alt, anchor_x, ly);
  }
}

struct AircraftDrawItem {
  size_t index = 0;
  int x = 0;
  int y = 0;
  int dist_sq = 0;
};

struct BeyondDotDrawItem {
  int x = 0;
  int y = 0;
  int dist_sq = 0;
};

void sortDrawItemsFarFirst(AircraftDrawItem* items, size_t count) {
  for (size_t i = 1; i < count; ++i) {
    const AircraftDrawItem key = items[i];
    size_t j = i;
    while (j > 0 && items[j - 1].dist_sq < key.dist_sq) {
      items[j] = items[j - 1];
      --j;
    }
    items[j] = key;
  }
}

void sortBeyondDotsFarFirst(BeyondDotDrawItem* items, size_t count) {
  for (size_t i = 1; i < count; ++i) {
    const BeyondDotDrawItem key = items[i];
    size_t j = i;
    while (j > 0 && items[j - 1].dist_sq < key.dist_sq) {
      items[j] = items[j - 1];
      --j;
    }
    items[j] = key;
  }
}

void drawAircraft() {
  initLabelMetrics();

  const size_t n = services::adsb::aircraftCount();
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();
  const uint32_t now_ms = millis();

  AircraftDrawItem items[services::adsb::kMaxAircraft];
  BeyondDotDrawItem dots[services::adsb::kMaxAircraft];
  size_t draw_count = 0;
  size_t dot_count = 0;

  for (size_t i = 0; i < n; ++i) {
    float draw_lat = planes[i].lat;
    float draw_lon = planes[i].lon;
    services::adsb::deadReckonPosition(planes[i], now_ms, &draw_lat, &draw_lon);

    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(draw_lat, draw_lon, &dx_km, &dy_km, &dist_km);

    if (isInsideOuterRingKm(dist_km)) {
      int x = 0;
      int y = 0;
      latLonToScreen(draw_lat, draw_lon, &x, &y);
      items[draw_count].index = i;
      items[draw_count].x = x;
      items[draw_count].y = y;
      items[draw_count].dist_sq = distSqFromCenter(x, y);
      ++draw_count;
      continue;
    }

    int dot_x = 0;
    int dot_y = 0;
    if (!beyondRingEdgeDotFromLatLon(draw_lat, draw_lon, &dot_x,
                                     &dot_y)) {
      continue;
    }
    dots[dot_count].x = dot_x;
    dots[dot_count].y = dot_y;
    dots[dot_count].dist_sq = distSqFromCenter(dot_x, dot_y);
    ++dot_count;
  }

  sortBeyondDotsFarFirst(dots, dot_count);
  for (size_t d = 0; d < dot_count; ++d) {
    drawBeyondRingDot(dots[d].x, dots[d].y);
  }

  sortDrawItemsFarFirst(items, draw_count);
  for (size_t d = 0; d < draw_count; ++d) {
    const size_t i = items[d].index;
    const int x = items[d].x;
    const int y = items[d].y;
    drawHeadingTriangle(x, y, planes[i].nose_deg, radar::kColorAircraft,
                        planes[i].type);
  }
  for (size_t d = 0; d < draw_count; ++d) {
    const size_t i = items[d].index;
    drawAircraftTag(items[d].x, items[d].y, planes[i]);
  }
}

void applyCardinalStyle() {
  if (s_cardinal_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_cardinal_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_cardinal_gfx);
  }
}

void applyScaleStyle() {
  if (s_scale_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_scale_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_scale_gfx);
  }
}

void drawCardinalLabel(const char* text, int x, int y, textdatum_t datum) {
  applyCardinalStyle();
  s_draw->setTextDatum(datum);
  s_draw->setTextColor(radar::kColorLabel, radar::kColorBackground);
  s_draw->drawString(text, x, y);
}

void drawScaleLabelWithBackground(const char* text, int x, int y) {
  applyScaleStyle();
  s_draw->setTextDatum(textdatum_t::middle_right);

  const int tw = s_draw->textWidth(text);
  const int th = s_draw->fontHeight();
  constexpr int kPadX = 3;
  constexpr int kPadY = 2;

  const int left = x - tw - kPadX;
  const int top = y - th / 2 - kPadY;

  s_draw->fillRect(left, top, tw + kPadX * 2, th + kPadY * 2,
                   radar::kColorBackground);
  s_draw->setTextColor(radar::kColorGrid, radar::kColorBackground);
  s_draw->drawString(text, x, y);
}

void drawGridRing(int cx, int cy, int r, uint16_t color) {
  if (r <= 0) {
    return;
  }
  const int thickness =
      std::max(1, static_cast<int>(radar::kGridStrokeHalfWidth * 2.0f));
  for (int i = 0; i < thickness && r - i > 0; ++i) {
    s_draw->drawCircle(cx, cy, r - i, color);
  }
}

void drawRings(int cx, int cy, int outer_radius) {
  for (int i = 1; i <= radar::kRingCount; ++i) {
    const int r = (outer_radius * i) / radar::kRingCount;
    drawGridRing(cx, cy, r, radar::kColorGrid);
  }
}

void drawCrosshairs(int cx, int cy, int radius, uint16_t color) {
  s_draw->drawWideLine(cx, cy - radius, cx, cy + radius,
                       radar::kGridStrokeHalfWidth, color);
  s_draw->drawWideLine(cx - radius, cy, cx + radius, cy,
                       radar::kGridStrokeHalfWidth, color);
}

void drawCenterDot(int cx, int cy) {
  s_draw->fillSmoothCircle(cx, cy, radar::kCenterDotRadius, radar::kColorCenter);
}

void drawCardinalLabels() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int edge = radar::kSize - 1;

  drawCardinalLabel("N", cx, radar::kCardinalNorthOffsetY, textdatum_t::top_center);
  drawCardinalLabel("S", cx, edge + radar::kCardinalSouthOffsetY,
                    textdatum_t::bottom_center);
  drawCardinalLabel("W", 0, cy, textdatum_t::middle_left);
  drawCardinalLabel("E", edge, cy, textdatum_t::middle_right);
}

int scaleLabelAnchorX(int cx, int outer_radius) {
  return cx + outer_radius - radar::kScaleGapFromOuterRing;
}

void drawScaleLabel(int cx, int cy, int outer_radius) {
  char scale_label[12];
  radar::formatCurrentRing3Label(scale_label, sizeof(scale_label));
  drawScaleLabelWithBackground(scale_label,
                               scaleLabelAnchorX(cx, outer_radius), cy);
}

template <typename Gfx>
void drawStaticGrid(Gfx& gfx) {
  initLabelMetrics();
  const DrawScope scope(gfx);
  displayFontEnsureLoaded(gfx);
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int grid_r = radar::kGridOuterRadius;

  gfx.fillScreen(radar::kColorBackground);
  drawRings(cx, cy, grid_r);
  drawCrosshairs(cx, cy, grid_r, radar::kColorGrid);
  initPalette();
  runway::drawLargeAirportRunways(gfx);
  drawCenterDot(cx, cy);
  drawCardinalLabels();
  drawScaleLabel(cx, cy, grid_r);
  gfx.setTextDatum(textdatum_t::top_left);
}

bool ensureFrameSprite() {
  if (s_frame_ready) {
    return true;
  }
  s_frame.setColorDepth(16);
  if (!s_frame.createSprite(radar::kSize, radar::kSize)) {
    Serial.println("radar: frame sprite alloc failed");
    return false;
  }
  s_frame_ready = true;
  return true;
}

// Double-buffered frame: composite the grid AND aircraft into the off-screen
// sprite, then blit it to the panel in a single pushSprite. Because the panel
// is updated in one pass, labels never show an erase/redraw gap — no flicker.
void renderFrame() {
  drawStaticGrid(s_frame);  // opens its own DrawScope(s_frame)
  {
    const DrawScope scope(s_frame);
    drawAircraft();
  }
  s_frame.pushSprite(0, 0);
  tft.setTextDatum(textdatum_t::top_left);
}

}  // namespace

void radarDisplayDraw() {
  initPalette();
  initLabelMetrics();

  if (ensureFrameSprite()) {
    renderFrame();
    return;
  }

  // Fallback when the sprite can't be allocated: draw straight to the panel.
  const DrawScope scope(tft);
  drawStaticGrid(tft);
  drawAircraft();
  tft.setTextDatum(textdatum_t::top_left);
}

void radarDisplayRefreshAircraft() {
  initPalette();

  if (ensureFrameSprite()) {
    renderFrame();
    return;
  }

  radarDisplayDraw();
}

}  // namespace ui
