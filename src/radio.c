#include "radio.h"
#include "config.h"
#include "apps/apps.h"
#include "board.h"
#include "dcs.h"
#include "driver/audio.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include "driver/si473x.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/uart.h"
#include "helper/adapter.h"
#include "helper/battery.h"
#include "helper/measurements.h"
#include "helper/vfos.h"
#include "misc.h"
#include "scheduler.h"
#include "settings.h"
#include "svc.h"
#include <stdint.h>
#include <string.h>

VFO *radio;
VFO gVFO[2] = {0};
// Preset *gVFOPresets[2] = {0};

char gVFONames[2][10] = {0};

bool gIsListening = false;
bool gMonitorMode = false;
TXState gTxState = TX_UNKNOWN;
bool gShowAllRSSI = false;

bool hasSI = false;

static Radio oldRadio = RADIO_UNKNOWN;
static uint32_t lastTailTone = 0;
static uint32_t lastMsmUpdate = 0;
static bool toneFound = false;

const uint16_t StepFrequencyTable[15] = {
    2,   5,   50,  100,

    250, 500, 625, 833, 900, 1000, 1250, 2500, 5000, 10000, 50000,
};

const char *modulationTypeOptions[8] = {"FM",  "AM",  "LSB", "USB",
                                        "BYP", "RAW", "WFM", "Preset"};
const char *powerNames[4] = {"ULOW, LOW", "MID", "HIGH"};
const char *bwNames[5] = {"25k", "12.5k", "6.25k", "25k+"};
const char *bwNamesSiAMFM[5] = {"6k", "4k", "3k", "1k"};
const char *bwNamesSiSSB[5] = {"4k", "3k", "2.2k", "0.5k"};
const char *radioNames[4] = {"BK4819", "BK1080", "SI4732", "Auto"};
const char *shortRadioNames[4] = {"BK", "BC", "SI", "PR"};
const char *TX_STATE_NAMES[7] = {"TX Off",   "TX On",  "CHARGING", "BAT LOW",
                                 "DISABLED", "UPCONV", "HIGH POW"};

const SquelchType sqTypeValues[4] = {
    SQUELCH_RSSI_NOISE_GLITCH,
    SQUELCH_RSSI_GLITCH,
    SQUELCH_RSSI_NOISE,
    SQUELCH_RSSI,
};
const char *sqTypeNames[4] = {"RNG", "RG", "RN", "R"};
const char *deviationNames[] = {"", "+", "-"};

static const SI47XX_SsbFilterBW SI_BW_MAP_SSB[] = {
    [BK4819_FILTER_BW_WIDE] = SI47XX_SSB_BW_4_kHz,
    [BK4819_FILTER_BW_NARROW] = SI47XX_SSB_BW_3_kHz,
    [BK4819_FILTER_BW_NARROWER] = SI47XX_SSB_BW_2_2_kHz,
    [BK4819_FILTER_BW_SOMETHING] = SI47XX_SSB_BW_0_5_kHz,
};
static const SI47XX_FilterBW SI_BW_MAP_AMFM[] = {
    [BK4819_FILTER_BW_WIDE] = SI47XX_BW_6_kHz,
    [BK4819_FILTER_BW_NARROW] = SI47XX_BW_4_kHz,
    [BK4819_FILTER_BW_NARROWER] = SI47XX_BW_3_kHz,
    [BK4819_FILTER_BW_SOMETHING] = SI47XX_BW_1_kHz,
};

void RADIO_HasSi() { hasSI = BK1080_ReadRegister(1) != 0x1080; }

Radio RADIO_Selector(uint32_t freq, ModulationType mod) {
  if (gVFO[gSettings.activeVFO].radio != RADIO_UNKNOWN) {
    return gVFO[gSettings.activeVFO].radio;
  }

  if ((freq > WFM_LOW && freq < WFM_HI) && mod == MOD_WFM ) {
    return hasSI ? RADIO_SI4732 : RADIO_BK1080;
  }

  if (freq > SI_BORDER) {
    return RADIO_BK4819;
  }

  if (freq > BEKEN_BORDER && mod != MOD_AM && !isPatchPresent){
    return RADIO_BK4819;
  }

  if ((freq > BEKEN_BORDER) && !(mod == MOD_AM || mod == MOD_LSB || mod == MOD_USB)) {
    return RADIO_BK4819;
  }
  
  return hasSI ? RADIO_SI4732 : RADIO_BK4819;
}

Radio RADIO_GetRadio() {
  return RADIO_Selector(radio->rx.f, RADIO_GetModulation());
}

ModulationType RADIO_GetModulation() {
  // return gCurrentPreset->band.modulation;
  return getNextModulation(false);
}

const char *RADIO_GetBWName(BK4819_FilterBandwidth_t i) {
  switch (RADIO_GetRadio()) {
  case RADIO_SI4732:
    if (RADIO_IsSSB()) {
      return bwNamesSiSSB[i];
    }
    return bwNamesSiAMFM[i];
  default:
    return bwNames[i];
  }
}

void RADIO_SetupRegisters(void) {
  /* GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, false);
  BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false); */
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
  // BK4819_SetupPowerAmplifier(0, 0); // 0 is default

  // BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE);

  while (BK4819_ReadRegister(BK4819_REG_0C) & 1U) {
    BK4819_WriteRegister(BK4819_REG_02, 0);
    SYSTEM_DelayMs(1);
  }
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  BK4819_WriteRegister(BK4819_REG_7D, 0xE94F | 10); // mic
  // TX
  // BK4819_WriteRegister(0x44, 38888);  // 300 resp TX
  BK4819_WriteRegister(0x74, 0xAF1F); // 3k resp TX

  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
  BK4819_WriteRegister(
      BK4819_REG_48,
      (11u << 12) |    // ??? .. 0 ~ 15, doesn't seem to make any difference
          (1u << 10) | // AF Rx Gain-1
          (56 << 4) |  // AF Rx Gain-2
          (8 << 0));   // AF DAC Gain (after Gain-1 and Gain-2)

  // BK4819_DisableScramble(); // default is off
  // BK4819_DisableVox() // default is off;
  BK4819_DisableDTMF();

  // BK4819_WriteRegister(BK4819_REG_3F, 0);
  /* BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_SQUELCH_FOUND |
                                          BK4819_REG_3F_SQUELCH_LOST); */
  BK4819_WriteRegister(0x40, (BK4819_ReadRegister(0x40) & ~(0b11111111111)) |
                                 1000 | (1 << 12));
  // BK4819_WriteRegister(0x40, (1 << 12) | (1450));
}

static void setSI4732Modulation(ModulationType mod) {
  if (!isPatchPresent) {
    switch (mod) {
    case MOD_WFM:
      SI47XX_SwitchMode(SI47XX_FM);
      return;
    default:
      SI47XX_SwitchMode(SI47XX_AM);
    }
    return;
  }
  switch (mod) {
    case MOD_AM:
      SI47XX_SwitchMode(SI47XX_AM);
      return;
    case MOD_LSB:
      SI47XX_SwitchMode(SI47XX_LSB);
      return;
    case MOD_USB:
      SI47XX_SwitchMode(SI47XX_USB);
      return;
    default: 
      SI47XX_SwitchMode(SI47XX_FM);
  }
}

static void onVfoUpdate(void) {
  TaskRemove(RADIO_SaveCurrentVFO);
  TaskAdd("VFO sav", RADIO_SaveCurrentVFO, 2000, false, 0);
}

// static void onPresetUpdate(void) {
//   TaskRemove(PRESETS_SaveCurrent);
//   TaskAdd("PRS sav", PRESETS_SaveCurrent, 2000, false, 0);
// }

static void setupToneDetection() {
  uint16_t InterruptMask =
      BK4819_REG_3F_CxCSS_TAIL | BK4819_REG_3F_DTMF_5TONE_FOUND;
  if (gSettings.dtmfdecode) {
    BK4819_EnableDTMF();
  } else {
    BK4819_DisableDTMF();
  }
  switch (radio->rx.codeType) {
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    // Log("DCS on");
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio->rx.codeType, radio->rx.code));
    InterruptMask |= BK4819_REG_3F_CDCSS_FOUND | BK4819_REG_3F_CDCSS_LOST;
    break;
  case CODE_TYPE_CONTINUOUS_TONE:
    // Log("CTCSS on");
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio->rx.code]);
    InterruptMask |= BK4819_REG_3F_CTCSS_FOUND | BK4819_REG_3F_CTCSS_LOST;
    break;
  default:
    // Log("STE on");
    BK4819_SetCTCSSFrequency(670);
    BK4819_SetTailDetection(550);
    break;
  }
  BK4819_WriteRegister(BK4819_REG_3F, InterruptMask);
}

static bool isSimpleSql() {
  return gSettings.sqlOpenTime == 0 && gSettings.sqlCloseTime == 0;
}

static bool isSqOpenSimple(uint16_t r) {
  const uint8_t band = radio->rx.f > SETTINGS_GetFilterBound() ? 1 : 0;
  const uint8_t sq = radio->squelch;
  const uint16_t ro = SQ[band][0][sq];
  const uint16_t rc = SQ[band][1][sq];
  const uint8_t no = SQ[band][2][sq];
  const uint8_t nc = SQ[band][3][sq];
  const uint8_t go = SQ[band][4][sq];
  const uint8_t gc = SQ[band][5][sq];

  uint8_t n, g;

  bool open;

  switch (radio->squelchType) {
  case SQUELCH_RSSI_NOISE_GLITCH:
    n = BK4819_GetNoise();
    g = BK4819_GetGlitch();
    open = r >= ro && n <= no && g <= go;
    if (r < rc || n > nc || g > gc) {
      open = false;
    }
    break;
  case SQUELCH_RSSI_NOISE:
    n = BK4819_GetNoise();
    open = r >= ro && n <= no;
    if (r < rc || n > nc) {
      open = false;
    }
    break;
  case SQUELCH_RSSI_GLITCH:
    g = BK4819_GetGlitch();
    open = r >= ro && g <= go;
    if (r < rc || g > gc) {
      open = false;
    }
    break;
  case SQUELCH_RSSI:
    open = r >= ro;
    if (r < rc) {
      open = false;
    }
    break;
  }

  return open;
  // return false;
}

static void toggleBK4819(bool on) {
  // Log("Toggle bk4819 audio %u", on);
  if (on) {
    BK4819_ToggleAFDAC(true);
    BK4819_ToggleAFBit(true);
    SYSTEM_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYSTEM_DelayMs(8);
    BK4819_ToggleAFDAC(false);
    BK4819_ToggleAFBit(false);
  }
}

static void toggleBK1080SI4732(bool on) {
  // Log("Toggle bk1080si audio %u", on);
  if (on) {
    SYSTEM_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYSTEM_DelayMs(8);
  }
}

static uint8_t calculateOutputPower(uint32_t f) {
  uint8_t power_bias;
  //
  // switch (p->power) {
  // case TX_POW_LOW:
  //   power_bias = p->powCalib.s;
  //   break;
  //
  // case TX_POW_MID:
  //   power_bias = p->powCalib.m;
  //   break;
  //
  // case TX_POW_HIGH:
  //   power_bias = p->powCalib.e;
  //   break;
  //
  // default:
  //   power_bias = p->powCalib.s;
  //   if (power_bias > 10)
  //     power_bias -= 10; // 10mw if Low=500mw
  // }
  power_bias = 10;

  return power_bias;
}

static void sendEOT() {
  BK4819_ExitSubAu();
  switch (gSettings.roger) {
  case 1:
    BK4819_PlayRoger();
    break;
  case 2:
    BK4819_PlayRogerTiny();
    break;
  case 3:
    BK4819_PlayRogerUgly();
    break;
  default:
    break;
  }
  if (gSettings.ste) {
    SYSTEM_DelayMs(50);
    BK4819_GenTail(4);
    BK4819_WriteRegister(BK4819_REG_51, 0x9033);
    SYSTEM_DelayMs(200);
  }
  BK4819_ExitSubAu();
}

static void rxTurnOff(Radio r) {
  switch (r) {
  case RADIO_BK4819:
    BK4819_Idle();
    break;
  case RADIO_BK1080:
    BK1080_Mute(true);
    break;
  case RADIO_SI4732:
    if (gSettings.si4732PowerOff) {
      SI47XX_PowerDown();
    } else {
      SI47XX_SetVolume(0);
    }
    break;
  default:
    break;
  }
}

static void rxTurnOn(Radio r) {
  switch (r) {
  case RADIO_BK4819:
    BK4819_RX_TurnOn();
    break;
  case RADIO_BK1080:
    BK4819_Idle();
    BK1080_Mute(false);
    BK1080_Init(radio->rx.f, true);
    break;
  case RADIO_SI4732:
    BK4819_Idle();
    if (gSettings.si4732PowerOff || !isSi4732On) {
      if (RADIO_IsSSB()) {
        SI47XX_PatchPowerUp();
      } else {
        SI47XX_PowerUp();
      }
    } else {
      SI47XX_SetVolume(63);
    }
    break;
  default:
    break;
  }
}

uint32_t GetScreenF(uint32_t f) { return f - gSettings.upconverter; }

uint32_t GetTuneF(uint32_t f) { return f + gSettings.upconverter; }

bool RADIO_IsSSB() {
  ModulationType mod = RADIO_GetModulation();
  return mod == MOD_LSB || mod == MOD_USB;
}

void RADIO_ToggleRX(bool on) {
  if (gIsListening == on) {
    return;
  }
  gRedrawScreen = true;

  gIsListening = on;

  if (on) {
    if (gSettings.backlightOnSquelch != BL_SQL_OFF) {
      BACKLIGHT_On();
    }
  } else {
    if (gSettings.backlightOnSquelch == BL_SQL_OPEN) {
      BACKLIGHT_Toggle(false);
    }
  }

  Radio r = RADIO_GetRadio();
  if (r == RADIO_BK4819) {
    toggleBK4819(on);
  } else {
    toggleBK1080SI4732(on);
  }
}

void RADIO_EnableCxCSS(void) {
  switch (radio->tx.codeType) {
  case CODE_TYPE_CONTINUOUS_TONE:
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio->tx.code]);
    break;
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio->tx.codeType, radio->tx.code));
    break;
  default:
    BK4819_ExitSubAu();
    break;
  }

  // SYSTEM_DelayMs(200);
}

uint32_t RADIO_GetTXFEx(VFO *vfo) {
  uint32_t txF = vfo->rx.f;

    txF = vfo->tx.f;

  return txF;
}

uint32_t RADIO_GetTXF(void) { return RADIO_GetTXFEx(radio); }

TXState RADIO_GetTXState(uint32_t txF) {
  if (gSettings.upconverter) {
    return TX_DISABLED_UPCONVERTER;
  }

  // const Preset *txPreset = PRESET_ByFrequency(txF);

  // if (!txPreset->allowTx || RADIO_GetRadio() != RADIO_BK4819 ||
  //     SVC_Running(SVC_FC)) {
  //   return TX_DISABLED;
  // }

  if (gBatteryPercent == 0) {
    return TX_BAT_LOW;
  }
  if (gChargingWithTypeC || gBatteryVoltage > 880) {
    return TX_VOL_HIGH;
  }
  return TX_ON;
}

uint32_t RADIO_GetTxPower(uint32_t txF) {
  // Preset *txPreset = PRESET_ByFrequency(txF);
  uint8_t power = calculateOutputPower(txF);
  if (power > 0x91) {
    power = 0x91;
  }
  // power >>= 2 - gCurrentPreset->power;
  return power;
}

bool RADIO_allowTX(uint32_t f) { return false; };

void RADIO_ToggleTX(bool on) {
  // uint32_t txF = RADIO_GetTXF();
  // uint8_t power = RADIO_GetTxPower(txF);
  // SVC_Toggle(SVC_FC, false, 0);
  // RADIO_ToggleTXEX(on, txF, power, true);
}

void RADIO_ToggleTXEX(bool on, uint32_t txF, uint8_t power, bool paEnabled) {
  bool lastOn = gTxState == TX_ON;
  if (gTxState == on) {
    return;
  }

  gTxState = on ? RADIO_GetTXState(txF) : TX_UNKNOWN;

  if (gTxState == TX_ON) {
    RADIO_ToggleRX(false);

    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

    BK4819_TuneTo(txF, true);

    BOARD_ToggleRed(gSettings.brightness > 1);
    BK4819_PrepareTransmit();

    SYSTEM_DelayMs(10);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, paEnabled);
    SYSTEM_DelayMs(5);
    BK4819_SetupPowerAmplifier(power, txF);
    SYSTEM_DelayMs(10);

    RADIO_EnableCxCSS();
  } else if (lastOn) {
    BK4819_ExitDTMF_TX(true); // also prepares to tx ste

    sendEOT();
    toggleBK1080SI4732(false);
    BOARD_ToggleRed(false);
    BK4819_TurnsOffTones_TurnsOnRX();

    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    setupToneDetection();
    BK4819_TuneTo(radio->rx.f, true);
  }
}

void RADIO_SetSquelchPure(uint32_t f, uint8_t sql) {
  BK4819_Squelch(sql, f, gSettings.sqlOpenTime, gSettings.sqlCloseTime);
}

void RADIO_TuneToPure(uint32_t f, bool precise) {
  // LOOT_Replace(&gLoot[gSettings.activeVFO], f);
  Radio r = RADIO_GetRadio();
  // Log("Tune %s to %u", radioNames[r], f);
  switch (r) {
  case RADIO_BK4819:
    BK4819_TuneTo(f, precise);
    break;
  case RADIO_BK1080:
    BK1080_SetFrequency(f);
    break;
  case RADIO_SI4732:
    SI47XX_TuneTo(f);
    break;
  default:
    break;
  }
}

void RADIO_SwitchRadio() {
  Radio r = RADIO_GetRadio();
  if (oldRadio == r) {
    return;
  }
  // Log("Switch radio from %u to %u", oldRadio + 1, r + 1);
  rxTurnOff(oldRadio);
  rxTurnOn(r);
  oldRadio = r;
}

void RADIO_SetupByCurrentVFO(void) {
  uint32_t f = radio->rx.f;
  lastMsmUpdate = 0;
  // PRESET_SelectByFrequency(f);
  // gVFOPresets[gSettings.activeVFO] = gCurrentPreset;

  /* Log("SetupByCurrentVFO, p=%s, r=%s, f=%u", gCurrentPreset->band.name,
      radioNames[RADIO_GetRadio()], f); */
  radio->modulation = getNextModulation(false);
  RADIO_SwitchRadio();

  RADIO_SetupBandParams();
  setupToneDetection();

  RADIO_TuneToPure(f, !gMonitorMode); // todo: precise when old preset !=new?
}

// USE CASE: set vfo temporary for current app
void RADIO_TuneTo(uint32_t f) {
  // if (radio->channel != -1) {
  //   radio->channel = -1;
  // }
  radio->tx.f = 0;
  radio->rx.f = f;
  radio->modulation = getNextModulation(false);
  radio->radio = RADIO_UNKNOWN;
  RADIO_SetupByCurrentVFO();
  setupToneDetection(); // note: idk where it will be
}

// USE CASE: set vfo and use in another app
void RADIO_TuneToSave(uint32_t f) {
  RADIO_TuneTo(f);
  RADIO_SaveCurrentVFO();
  // radio->lastUsedFreq = f;
  // PRESETS_SaveCurrent();
}

void RADIO_SaveCurrentVFO(void) { VFOS_Save(gSettings.activeVFO, radio); }

// void RADIO_SelectPresetSave(int8_t num) {
//   PRESET_Select(num);
//   radio->modulation = gCurrentPreset->band.modulation;
//   radio->modulation = getNextModulation(false);
//   radio->radio = RADIO_UNKNOWN;
//
//   // PRESETS_SaveCurrent();
//   if (PRESET_InRange(gCurrentPreset->lastUsedFreq, gCurrentPreset)) {
//     RADIO_TuneToSave(gCurrentPreset->lastUsedFreq);
//   } else {
//     RADIO_TuneToSave(gCurrentPreset->band.bounds.start);
//   }
// }

void RADIO_LoadCurrentVFO(void) {
  for (uint8_t i = 0; i < 2; ++i) {
    VFOS_Load(i, &gVFO[i]);
  }

  radio = &gVFO[gSettings.activeVFO];
  RADIO_SetupByCurrentVFO();
}

void RADIO_SetSquelch(uint8_t sq) {
  radio->squelch = sq;
  RADIO_SetSquelchPure(radio->rx.f, sq);
  RADIO_SaveCurrentVFO();
}

void RADIO_SetSquelchType(SquelchType t) {
  radio->squelchType = t;
  RADIO_SaveCurrentVFO();
}

void RADIO_SetGain(uint8_t gainIndex) {
  // gCurrentPreset->band.gainIndex = gainIndex;
  bool disableAGC = false;
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    // BK4819_SetAGC(gCurrentPreset->band.modulation != MOD_AM, gainIndex);
    break;
  case RADIO_SI4732:
    // 0 - max gain
    // 26 - min gain
    gainIndex = ARRAY_SIZE(gainTable) - 1 - gainIndex;
    disableAGC = gainIndex != 0;
    gainIndex = ConvertDomain(gainIndex, 0, ARRAY_SIZE(gainTable) - 1, 0, 26);
    SI47XX_SetAutomaticGainControl(disableAGC, gainIndex);
    break;
  case RADIO_BK1080:
    break;
  default:
    break;
  }
  // onPresetUpdate();
}

void RADIO_SetFilterBandwidth(BK4819_FilterBandwidth_t bw) {
  ModulationType mod = RADIO_GetModulation();
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    BK4819_SetFilterBandwidth(bw);
    break;
  case RADIO_BK1080:
    break;
  case RADIO_SI4732:
    if (mod == MOD_USB || mod == MOD_LSB) {
      SI47XX_SetSsbBandwidth(SI_BW_MAP_SSB[bw]);
    } else {
      SI47XX_SetBandwidth(SI_BW_MAP_AMFM[bw], true);
    }
    break;
  default:
    break;
  }
}

void RADIO_SetupBandParams() {
  // Log("RADIO_SetupBandParams");
  // Band *b = ;
  // uint32_t fMid = b->bounds.start + (b->bounds.end - b->bounds.start) / 2;
  ModulationType mod = RADIO_GetModulation();
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    BK4819_SquelchType(radio->squelchType);
    BK4819_Squelch(radio->squelch, radio->rx.f, gSettings.sqlOpenTime,
                   gSettings.sqlCloseTime);
    BK4819_SetModulation(mod);
    if (gSettings.scrambler) {
      BK4819_EnableScramble(gSettings.scrambler);
    } else {
      BK4819_DisableScramble();
    }
    // BK4819_RX_TurnOn(); /needed?
    break;
  case RADIO_BK1080:
    break;
  case RADIO_SI4732:
    setSI4732Modulation(mod);
    if (mod == MOD_FM) {
      // SI47XX_SetSeekFmLimits(b->bounds.start, b->bounds.end);
      SI47XX_SetSeekFmSpacing(StepFrequencyTable[radio->step]);
    } else {
      if (mod == MOD_AM) {
        // SI47XX_SetSeekAmLimits(b->bounds.start, b->bounds.end);
        SI47XX_SetSeekAmSpacing(StepFrequencyTable[radio->step]);
      }
    }

    break;
  default:
    break;
  }
  RADIO_SetGain(radio->gainIndex);
  // Log("Set mod %s", modulationTypeOptions[mod]);
  RADIO_SetFilterBandwidth(radio->bw);
  // Log("RADIO_SetupBandParams end");
}

uint16_t RADIO_GetRSSI(void) {
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return BK4819_GetRSSI();
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetRSSI() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return ConvertDomain(rsqStatus.resp.RSSI, 0, 64, 30, 346);
    }
    return 0;
  default:
    return 128;
  }
}

uint16_t RADIO_GetSNR(void) {
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return BK4819_GetSNR();
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetSNR() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return rsqStatus.resp.SNR;
    }
    return 0;
  default:
    return 0;
  }
}

uint16_t RADIO_GetS() {
    uint8_t snr = RADIO_GetSNR();
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return ConvertDomain(snr, 0, 137, 0, 13);
  case RADIO_BK1080:
    return ConvertDomain(snr, 0, 137, 0, 13);
  case RADIO_SI4732:
    return ConvertDomain(snr, 0, 30, 0, 13);
  default:
    return 0;
  }
}

bool RADIO_IsSquelchOpen() {
  if (RADIO_GetRadio() == RADIO_BK4819) {
    if (isSimpleSql()) {
      return isSqOpenSimple(radio->rssi);
    } else {
      return BK4819_IsSquelchOpen();
    }
  }
  return true;
}

void RADIO_UpdateMeasurements(void) {
  if (RADIO_GetRadio() == RADIO_SI4732 && SVC_Running(SVC_SCAN)) {
    bool valid = false;
    uint32_t f = SI47XX_getFrequency(&valid);
    radio->rx.f = f;
    gRedrawScreen = true;
    if (valid) {
      SVC_Toggle(SVC_SCAN, false, 0);
    }
  }
  if (RADIO_GetRadio() != RADIO_BK4819 && Now() - lastMsmUpdate <= 1000) {
    return;
  }
  lastMsmUpdate = Now();
  radio->rssi = RADIO_GetRSSI();
  radio->open = RADIO_IsSquelchOpen();
  if (radio->rx.codeType == CODE_TYPE_OFF) {
    toneFound = true;
  }

  if (RADIO_GetRadio() == RADIO_BK4819) {
    while (BK4819_ReadRegister(BK4819_REG_0C) & 1u) {
      BK4819_WriteRegister(BK4819_REG_02, 0);

      uint16_t intBits = BK4819_ReadRegister(BK4819_REG_02);

      if ((intBits & BK4819_REG_02_CxCSS_TAIL) ||
          (intBits & BK4819_REG_02_CTCSS_FOUND) ||
          (intBits & BK4819_REG_02_CDCSS_FOUND)) {
        // Log("Tail tone or ctcss/dcs found");
        radio->open = false;
        toneFound = false;
        lastTailTone = Now();
      }
      if ((intBits & BK4819_REG_02_CTCSS_LOST) ||
          (intBits & BK4819_REG_02_CDCSS_LOST)) {
        // Log("ctcss/dcs lost");
        radio->open = true;
        toneFound = true;
      }

      /* if (intBits & BK4819_REG_02_DTMF_5TONE_FOUND) {
        uint8_t code = BK4819_GetDTMF_5TONE_Code();
        Log("DTMF: %u", code);
      } */
    }
    // else sql reopens
    if (!toneFound || (Now() - lastTailTone) < 250) {
       radio->open = false;
    }
  }

  bool rx = radio->open;
  if (gTxState != TX_ON) {
    if (gMonitorMode) {
      rx = true;
    } else if (gSettings.noListen
               && (gCurrentApp == APP_SPECTRUM /*|| gCurrentApp == APP_ANALYZER*/)
    ){
      rx = false;
    } else if (gSettings.skipGarbageFrequencies &&
               (radio->rx.f % 1300000 == 0) &&
               RADIO_GetRadio() == RADIO_BK4819) {
      rx = false;
    }
    RADIO_ToggleRX(rx);
  }
}

void UpdateEx() {

  if (SVC_Running(SVC_SCAN) && !gMonitorMode) {
    radio->open = false;
  }

  if (radio->open) {
    uint32_t cd = 0;
    uint16_t ct = 0;
    uint8_t Code = 0;
    BK4819_CssScanResult_t res = BK4819_GetCxCSSScanResult(&cd, &ct);
    switch (res) {
    case BK4819_CSS_RESULT_CDCSS:
      Code = DCS_GetCdcssCode(cd);
      if (Code != 0xFF) {
        radio->cd = Code;
      }
      break;
    case BK4819_CSS_RESULT_CTCSS:
      Code = DCS_GetCtcssCode(ct);
      if (Code != 0xFF) {
        radio->ct = Code;
      }
      break;
    default:
      break;
    }
    radio->lastTimeOpen = Now();
  }
}

bool RADIO_UpdateMeasurementsEx() {
  VFO *msm = &gVFO[gSettings.activeVFO];
  RADIO_UpdateMeasurements();
  UpdateEx();
  return msm->open;
}

// void RADIO_VfoLoadCH(uint8_t i) {
//   CH ch;
//   CHANNELS_Load(gVFO[i].channel, &ch);
//   CH2VFO(&ch, &gVFO[i]);
//   strncpy(gVFONames[i], ch.name, 9);
// }
//
// bool RADIO_TuneToCH(int32_t num) {
//   if (CHANNELS_Existing(num)) {
//     radio->channel = num;
//     RADIO_VfoLoadCH(gSettings.activeVFO);
//     onVfoUpdate();
//     RADIO_SetupByCurrentVFO();
//     return true;
//   }
//   return false;
// }
//
// void RADIO_NextCH(bool next) {
//   RADIO_TuneToCH(CHANNELS_Next(radio->channel, next));
// }

void RADIO_NextVFO(void) {
  gSettings.activeVFO = !gSettings.activeVFO;
  radio = &gVFO[gSettings.activeVFO];
  RADIO_SetupByCurrentVFO();
  SETTINGS_Save();
}

// void RADIO_ToggleVfoMR(void) {
//   if (radio->channel >= 0) {
//     radio->channel += 1; // 0 -> 1
//     radio->channel *= -1;
//   } else {
//     radio->channel *= -1;
//     radio->channel -= 1; // 1 -> 0
//     if (CHANNELS_Existing(radio->channel)) {
//       RADIO_TuneToCH(radio->channel);
//     } else {
//       RADIO_NextCH(true);
//     }
//   }
//   RADIO_SaveCurrentVFO();
// }

void RADIO_UpdateSquelchLevel(bool next) {
  uint8_t sq = radio->squelch;
  IncDec8(&sq, 0, 10, next ? 1 : -1);
  RADIO_SetSquelch(sq);
}

void RADIO_NextFreqNoClicks(bool next) {
  const int8_t dir = next ? 1 : -1;
  radio->tx.f = 0;
  radio->rx.f += StepFrequencyTable[radio->step] * (next ? 1 : -1);
  RADIO_SetupByCurrentVFO();
  onVfoUpdate();
}

// static void selectPreset(bool next) {
//   if (gSettings.crossBandScan) {
//     uint8_t index = gSettings.activePreset;
//     uint8_t sl = gSettings.currentScanlist;
//     uint8_t scanlistMask = 1 << sl;
//     PRESETS_SelectPresetRelative(next);
//     while (gSettings.activePreset != index) {
//       if (sl == 15 ||
//           (gCurrentPreset->memoryBanks & scanlistMask) == scanlistMask) {
//         return;
//       }
//       PRESETS_SelectPresetRelative(next);
//     }
//   }
// }

bool RADIO_NextFreqXBandEx(bool next, bool tune, bool precise) {
  int64_t step = radio->step;
  bool switchBand = false;

  if (next) {
    radio->rx.f+=step;
  } else {
    radio->rx.f-=step;
  }

  if (tune) {
    RADIO_TuneToPure(radio->rx.f, precise);
  }
  return switchBand;
}

void RADIO_NextFreqXBand(bool next) {
  RADIO_NextFreqXBandEx(next, true, true);
}

static ModulationType MODS_BK4819[] = {
    MOD_FM, MOD_AM, MOD_USB, MOD_BYP, MOD_RAW, MOD_WFM,
};

static ModulationType MODS_BOTH_PATCH[] = {
    MOD_FM, MOD_AM, MOD_USB, MOD_LSB, MOD_BYP, MOD_RAW, MOD_WFM,
};

static ModulationType MODS_BOTH[] = {
    MOD_FM, MOD_AM, MOD_USB, MOD_BYP, MOD_RAW, MOD_WFM,
};

static ModulationType MODS_BK1080[] = {
    MOD_WFM,
};

static ModulationType MODS_SI4732_PATCH[] = {
    MOD_AM,
    MOD_LSB,
    MOD_USB,
};

static ModulationType MODS_SI4732[] = {
    MOD_AM,
};

static int8_t indexOf(ModulationType *arr, uint8_t n, ModulationType t) {
  for (uint8_t i = 0; i < n; ++i) {
    if (arr[i] == t) {
      return i;
    }
  }
  return 0;
}

ModulationType getNextModulation(bool next) {
  uint8_t sz;
  ModulationType *items;

    if (radio->rx.f < SI_BORDER && radio->rx.f > BEKEN_BORDER){
      if (isPatchPresent){
        items = MODS_BOTH_PATCH;
        sz = ARRAY_SIZE(MODS_BOTH_PATCH);
      }else{
        items = MODS_BOTH;
        sz = ARRAY_SIZE(MODS_BOTH);
      }
    }else if (radio->rx.f <= BEKEN_BORDER) {
      if (isPatchPresent){
        items = MODS_SI4732_PATCH;
        sz = ARRAY_SIZE(MODS_SI4732_PATCH);
      }else {
        items = MODS_SI4732;
        sz = ARRAY_SIZE(MODS_SI4732);
      }
    }else{
        items = MODS_BK4819;
        sz = ARRAY_SIZE(MODS_BK4819);
    }

  int8_t curIndex = indexOf(items, sz, radio->modulation);
  
  if (next) IncDecI8(&curIndex, 0, sz, 1);

  return items[curIndex];
}

void RADIO_ToggleModulation(void) {
  if (radio->modulation == getNextModulation(true)) return;
  radio->modulation = getNextModulation(true);

  // NOTE: for right BW after switching from WFM to another
  RADIO_SetupBandParams();
  onVfoUpdate();
}

void RADIO_UpdateStep(bool inc) {
  uint8_t step = radio->step;
  IncDec8(&step, 0, STEP_500_0kHz, inc ? 1 : -1);
  radio->step = step;
  RADIO_SaveCurrentVFO();
}

void RADIO_ToggleListeningBW(void) {
  // if (gCurrentPreset->band.bw == BK4819_FILTER_BW_SOMETHING) {
  //   gCurrentPreset->band.bw = BK4819_FILTER_BW_WIDE;
  // } else {
  //   ++gCurrentPreset->band.bw;
  // }
  //
  // RADIO_SetFilterBandwidth(gCurrentPreset->band.bw);
  //
  // onPresetUpdate();
}

void RADIO_ToggleTxPower(void) {
  // if (gCurrentPreset->power == TX_POW_HIGH) {
  //   gCurrentPreset->power = TX_POW_ULOW;
  // } else {
  //   ++gCurrentPreset->power;
  // }
  //
  // onPresetUpdate();
}
