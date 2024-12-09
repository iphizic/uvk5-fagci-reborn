#include "reset.h"
#include "../radio.h"
#include "../driver/eeprom.h"
#include "../driver/st7565.h"
// #include "../helper/channels.h"
#include "../helper/measurements.h"
// #include "../helper/presetlist.h"
#include "../helper/vfos.h"
#include "../settings.h"
#include "../ui/graphics.h"
#include "ARMCM0.h"

static uint32_t bytesMax = 0;
static uint32_t bytesWrote = 0;
// static uint16_t channelsMax = 0;
// static uint16_t channelsWrote = 0;
// static uint8_t presetsWrote = 0;
static uint8_t vfosWrote = 0;
static bool settingsWrote = 0;

static EEPROMType eepromType;

static VFO defaultVFOs[2] = {
    (VFO){
        .rx.f = 14550000,
        .modulation = MOD_FM,
        .radio = RADIO_UNKNOWN,
        .step = STEP_1_0kHz,
    },
    (VFO){
        .rx.f = 43307500,
        .modulation = MOD_FM,
        .radio = RADIO_UNKNOWN,
        .step = STEP_1_0kHz,
    },
};

static void startReset(EEPROMType t) {
  eepromType = t;
  gSettings.eepromType = eepromType;
  // presetsWrote = 0;
  vfosWrote = 0;
  bytesWrote = 0;
  // channelsWrote = 0;
  settingsWrote = false;
  // channelsMax = CHANNELS_GetCountMax();
  bytesMax = SETTINGS_SIZE + VFO_SIZE * 2;
}

void RESET_Init(void) {
  gSettings.keylock = false;
  eepromType = EEPROM_A;
}

void RESET_Update(void) {
  if (eepromType < EEPROM_BL24C64) {
    return;
  }
  if (!settingsWrote) {
    gSettings = (Settings){
        .eepromType = eepromType,
        .squelch = 4,
        .scrambler = 0,
        .batsave = 4,
        .vox = 0,
        .backlight = BL_TIME_VALUES[3],
        .txTime = 0,
        .micGain = 15,
        .currentScanlist = 15,
        .roger = 0,
        .scanmode = 0,
        .chDisplayMode = 0,
        .dw = false,
        .crossBandScan = false,
        .beep = false,
        .keylock = false,
        .busyChannelTxLock = false,
        .ste = true,
        .repeaterSte = true,
        .dtmfdecode = false,
        .brightness = 8,
        .contrast = 8,
        .mainApp = APP_VFO2,
        .sqOpenedTimeout = SCAN_TO_NONE,
        .sqClosedTimeout = SCAN_TO_2s,
        .sqlOpenTime = 1,
        .sqlCloseTime = 1,
        .skipGarbageFrequencies = true,
        .scanTimeout = 50,
        .activeVFO = 0,
        // .activePreset = 9,
        // .presetsCount = ARRAY_SIZE(defaultPresets),
        .backlightOnSquelch = BL_SQL_ON,
        .batteryCalibration = 2000,
        .batteryType = BAT_1600,
        .batteryStyle = BAT_PERCENT,
        .upconverter = 0,
    };
    settingsWrote = true;
    bytesWrote += SETTINGS_SIZE;
  } else if (vfosWrote < ARRAY_SIZE(defaultVFOs)) {
    VFOS_Save(vfosWrote, &defaultVFOs[vfosWrote]);
    vfosWrote++;
    bytesWrote += VFO_SIZE;
  // } else if (presetsWrote < ARRAY_SIZE(defaultPresets)) {
  //   Preset *p = &defaultPresets[presetsWrote];
  //   p->band.gainIndex = 18;
  //   p->band.squelch = 3;
  //   p->band.squelchType = SQUELCH_RSSI_NOISE_GLITCH;
  //   if (p->band.bounds.end < 3000000) {
  //     p->radio = RADIO_SI4732; // TODO: if SI existing
  //                              // default is RADIO_BK4819
  //   }
  //   PRESETS_SavePreset(presetsWrote, p);
  //   presetsWrote++;
  //   bytesWrote += PRESET_SIZE;
  // } else if (channelsWrote < channelsMax) {
  //   CHANNELS_Delete(channelsWrote);
  //   channelsWrote++;
  //   bytesWrote += CH_SIZE;
  } else {
    SETTINGS_Save();
    NVIC_SystemReset();
  }
  gRedrawScreen = true;
}

void RESET_Render(void) {
  uint8_t progressX = ConvertDomain(bytesWrote, 0, bytesMax, 1, LCD_WIDTH - 2);
  uint8_t POS_Y = LCD_HEIGHT / 2;

  if (eepromType < EEPROM_BL24C64) {
    for (uint8_t t = EEPROM_BL24C64; t <= EEPROM_M24M02; t++) {
      uint8_t i = t - EEPROM_BL24C64;
      PrintMedium(2, 18 + i * 8, "%u: %s", i + 1, EEPROM_TYPE_NAMES[t]);
    }
    return;
  }

  DrawRect(0, POS_Y, LCD_WIDTH, 10, C_FILL);
  FillRect(1, POS_Y, progressX, 10, C_FILL);
  // PrintMedium(0, 16, "%u/%u", channelsWrote, channelsMax);
  PrintMedium(0, 24, "%lu", bytesMax);
  PrintMediumEx(LCD_XCENTER, POS_Y + 8, POS_C, C_INVERT, "%u%",
                bytesWrote * 100 / bytesMax);
}

bool RESET_key(KEY_Code_t k, bool bKeyPressed, bool bKeyHeld) {
  if (!bKeyPressed && !bKeyHeld && k > KEY_0) {
    uint8_t t = EEPROM_BL24C64 + k - 1;
    if (t < ARRAY_SIZE(EEPROM_TYPE_NAMES)) {
      startReset(t);
      return true;
    }
  }
  return false;
}
