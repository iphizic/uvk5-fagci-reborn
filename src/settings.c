#include "settings.h"
#include "helper/mem_manager.h"
#include "scheduler.h"
#include <stdint.h>
#include <string.h>

Settings gSettings;
DwState gDW;

bool isPatchPresent = false;

uint8_t BL_TIME_VALUES[7] = {0, 5, 10, 20, 60, 120, 255};
const char *BL_TIME_NAMES[7] = {"Off",  "5s",   "10s", "20s",
                                "1min", "2min", "On"};

const char *BL_SQL_MODE_NAMES[3] = {"Off", "On", "Open"};
const char *CH_DISPLAY_MODE_NAMES[3] = {"Name+F", "F", "Name"};
const char *TX_POWER_NAMES[4] = {"ULow", "Low", "Mid", "High"};
const char *TX_OFFSET_NAMES[3] = {"None", "+", "-"};
const char *TX_CODE_TYPES[4] = {"None", "CT", "DCS", "-DCS"};
const char *rogerNames[4] = {"None", "Moto", "Tiny", "Call"};
const char *dwNames[3] = {"Off", "TX Stay", "TX Switch"};
const char *EEPROM_TYPE_NAMES[8] = {
    "-",         // 000
    "-",         // 001
    "BL24C64 #", // 010
    "BL24C128",  // 011
    "BL24C256",  // 100
    "BL24C512",  // 101
    "BL24C1024", // 110
    "M24M02",    // 111
};

const uint32_t EEPROM_SIZES[8] = {
    8192,   // 000
    8192,   // 001
    8192,   // 010
    16384,  // 011
    32768,  // 100
    65536,  // 101
    131072, // 110
    262144, // 111
};

const uint16_t PAGE_SIZES[8] = {
    32,  // 000
    32,  // 001
    32,  // 010
    64,  // 011
    64,  // 100
    128, // 101
    128, // 110
    256, // 111
};

void SETTINGS_checkSSBPatch(){
  // if (SETTINGS_GetEEPROMSize() < 32768) {
  //   isPatchPresent = false;
  //   return;
  // }
  // uint8_t buf[8];
  // const uint8_t patch[8] = PATCH_PREAMBULE;
  // const uint32_t PATCH_START = SETTINGS_GetEEPROMSize() - PATCH_SIZE;
  // EEPROM_ReadBuffer(PATCH_START, buf, 8);
  // for(uint32_t i=0; i < 8; i++){
  //    if (patch[i] != buf[i]){
  //     isPatchPresent = false;
  //     return;
  //   }
  // }
  // isPatchPresent = true;
}

void SETTINGS_Save(void) {
  
}

void SETTINGS_Load() {
  Options opts = MEMMANAGER_getOptions(0);
  MemBindata buf;
  for(uint8_t i = 0; i < MAX_OPTIONS_LENGTH; i++){
    Option opt = opts.options[i];
    switch (opt.option) {
      case eepromType:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case checkbyte:
        gSettings.checkbyte = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case squelch:
        gSettings.squelch = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case scrambler:
        gSettings.scrambler = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case batsave:
        gSettings.batsave = MEMMANAGER_read_uint8_t(opt.address);
        break;
 
      case vox:
        gSettings.vox = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case backlight:
        gSettings.backlight = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case txTime:
        gSettings.txTime = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case micGain:
        gSettings.micGain = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case currentScanlist:
        gSettings.currentScanlist = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case roger:
        gSettings.roger = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case scanmode:
        gSettings.scanmode = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case chDisplayMode:

        break;

      case pttLock:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case crossBandScan:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case beep:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case keylock:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case busyChannelTxLock:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case ste:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case repeaterSte:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case dtmfdecode:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case brightness:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case contrast:
        gSettings.eepromType = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case mainApp:
        
        break;

      // int8_t presetsCount = 24,
      // int8_t activePreset = 25,
      case batteryCalibration:
        gSettings.batteryCalibration = MEMMANAGER_read_uint16_t(opt.address);
        break;

      case batteryType:

        break;
        
      case batteryStyle:

        break;

      case sqOpenedTimeout:

        break;

      case sqClosedTimeout:

        break;

      case bound_240_280:
        gSettings.bound_240_280 = MEMMANAGER_read_bool(opt.address);
        break;

      case noListen:
        gSettings.noListen = MEMMANAGER_read_bool(opt.address);
        break;

      case si4732PowerOff:
        gSettings.si4732PowerOff = MEMMANAGER_read_bool(opt.address);
        break;

      case dw:
        gSettings.dw = MEMMANAGER_read_bool(opt.address);
        break;


      case toneLocal:
        gSettings.toneLocal = MEMMANAGER_read_bool(opt.address);
        break;

      case backlightOnSquelch:

        break;

      case scanTimeout:
        gSettings.scanTimeout = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case sqlOpenTime: 
        gSettings.sqlOpenTime = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case sqlCloseTime:
        gSettings.sqlCloseTime = MEMMANAGER_read_uint8_t(opt.address);
        break;
        
      case skipGarbageFrequencies:
        gSettings.skipGarbageFrequencies = MEMMANAGER_read_bool(opt.address);
        break;

      case activeVFO:
        gSettings.activeVFO = MEMMANAGER_read_uint8_t(opt.address);
        break;

      case upconverter:
        gSettings.upconverter = MEMMANAGER_read_uint32_t(opt.address);
        break;

      default:
        break;       
    }
  }
  
}

void SETTINGS_DelayedSave(void) {
  TaskRemove(SETTINGS_Save);
  TaskAdd("SetSav", SETTINGS_Save, 5000, false, 0);
}

uint32_t SETTINGS_GetFilterBound(void) {
  return gSettings.bound_240_280 ? VHF_UHF_BOUND2 : VHF_UHF_BOUND1;
}

uint32_t SETTINGS_GetEEPROMSize(void) {
  return EEPROM_SIZES[gSettings.eepromType];
}

uint16_t SETTINGS_GetPageSize(void) { return PAGE_SIZES[gSettings.eepromType]; }
