#include "menu.h"
#include "../dcs.h"
#include "../driver/st7565.h"
#include "../helper/measurements.h"
#include "graphics.h"
#include "../radio.h"

const char *onOff[] = {"Off", "On"};
const char *yesNo[] = {"No", "Yes"};

void UI_DrawScrollBar(const uint16_t size, const uint16_t iCurrent,
                      const uint8_t nLines) {
  const uint8_t sbY =
      ConvertDomain(iCurrent, 0, size, 0, nLines * MENU_ITEM_H - 3);

  DrawVLine(LCD_WIDTH - 2, MENU_Y, LCD_HEIGHT - MENU_Y, C_FILL);

  FillRect(LCD_WIDTH - 3, MENU_Y + sbY, 3, 3, C_FILL);
}

void UI_ShowMenuItem(uint8_t line, const char *name, bool isCurrent) {
  uint8_t by = MENU_Y + line * MENU_ITEM_H + 8;
  PrintMedium(4, by, "%s", name);
  if (isCurrent) {
    FillRect(0, MENU_Y + line * MENU_ITEM_H, LCD_WIDTH - 3, MENU_ITEM_H,
             C_INVERT);
  }
}

void UI_ShowMenuSimple(const MenuItem *menu, uint16_t size,
                       uint16_t currentIndex) {
  const uint16_t maxItems =
      size < MENU_LINES_TO_SHOW ? size : MENU_LINES_TO_SHOW;
  const uint16_t offset = Clamp(currentIndex - 2, 0, size - maxItems);
  char name[32] = "";

  for (uint16_t i = 0; i < maxItems; ++i) {
    uint16_t itemIndex = i + offset;
    strncpy(name, menu[itemIndex].name, 31);
    PrintSmallEx(LCD_WIDTH - 4, MENU_Y + i * MENU_ITEM_H + 8, POS_R, C_FILL,
                 "%u", itemIndex + 1);
    UI_ShowMenuItem(i, name, currentIndex == itemIndex);
  }

  UI_DrawScrollBar(size, currentIndex, MENU_LINES_TO_SHOW);
}

void UI_ShowMenu(void (*getItemText)(uint16_t index, char *name), uint16_t size,
                 uint16_t currentIndex) {
  const uint16_t maxItems =
      size < MENU_LINES_TO_SHOW ? size : MENU_LINES_TO_SHOW;
  const uint16_t offset = Clamp(currentIndex - 2, 0, size - maxItems);

  for (uint16_t i = 0; i < maxItems; ++i) {
    char name[32] = "";
    uint16_t itemIndex = i + offset;
    getItemText(itemIndex, name);
    PrintSmallEx(LCD_WIDTH - 4, MENU_Y + i * MENU_ITEM_H + 8, POS_R, C_FILL,
                 "%u", itemIndex + 1);
    UI_ShowMenuItem(i, name, currentIndex == itemIndex);
  }

  UI_DrawScrollBar(size, currentIndex, MENU_LINES_TO_SHOW);
}

void UI_ShowMenuEx(void (*showItem)(uint16_t i, uint16_t index, bool isCurrent),
                   uint16_t size, uint16_t currentIndex, uint16_t linesMax) {
  const uint16_t maxItems = size < linesMax ? size : linesMax;
  const uint16_t offset = Clamp(currentIndex - 2, 0, size - maxItems);

  for (uint16_t i = 0; i < maxItems; ++i) {
    uint16_t itemIndex = i + offset;
    showItem(i, itemIndex, currentIndex == itemIndex);
  }

  UI_DrawScrollBar(size, currentIndex, linesMax);
}

void PrintRTXCode(char *Output, uint8_t codeType, uint8_t code) {
  if (codeType) {
    if (codeType == CODE_TYPE_CONTINUOUS_TONE) {
      sprintf(Output, "CT:%u.%uHz", CTCSS_Options[code] / 10,
              CTCSS_Options[code] % 10);
    } else if (codeType == CODE_TYPE_DIGITAL) {
      sprintf(Output, "DCS:D%03oN", DCS_Options[code]);
    } else if (codeType == CODE_TYPE_REVERSE_DIGITAL) {
      sprintf(Output, "DCS:D%03oI", DCS_Options[code]);
    } else {
      sprintf(Output, "No code");
    }
  }
}

void GetMenuItemValue(PresetCfgMenu type, char *Output) {
  // Band *band = &gVFO->band;
  // uint32_t fs = band->bounds.start;
  // uint32_t fe = band->bounds.end;
  bool isVfo = gCurrentApp == APP_VFO_CFG;
  switch (type) {
  case M_RADIO:
    strncpy(Output, radioNames[radio->radio],
            31);
    break;
  // case M_START:
  //   sprintf(Output, "%lu.%03lu", fs / MHZ, fs / 100 % 1000);
  //   break;
  // case M_END:
  //   sprintf(Output, "%lu.%03lu", fe / MHZ, fe / 100 % 1000);
  //   break;
  // case M_NAME:
  //   strncpy(Output, band->name, 31);
  //   break;
  // case M_BW:
  //   strncpy(Output, RADIO_GetBWName(band->bw), 31);
  //   break;
  case M_SQ_TYPE:
    strncpy(Output, sqTypeNames[gVFO->squelchType], 31);
    break;
  case M_SQ:
    sprintf(Output, "%u", gVFO->squelch);
    break;
  case M_GAIN:
    sprintf(Output, "%ddB", gainTable[gVFO->gainIndex].gainDb);
    break;
  case M_MODULATION:
    strncpy(Output, modulationTypeOptions[radio->modulation], 31);
    break;
  case M_STEP:
    sprintf(Output, "%u.%02uKHz", StepFrequencyTable[gVFO->step] / 100,
            StepFrequencyTable[gVFO->step] % 100);
    break;
  case M_TX:
    // strncpy(Output, yesNo[gCurrentPreset->allowTx], 31);
    break;
  case M_F_RX:
    sprintf(Output, "%u.%05u", radio->rx.f / MHZ, radio->rx.f % MHZ);
    break;
  case M_F_TX:
    sprintf(Output, "%u.%05u", radio->tx.f / MHZ, radio->tx.f % MHZ);
    break;
  case M_RX_CODE_TYPE:
    strncpy(Output, TX_CODE_TYPES[radio->rx.codeType], 31);
    break;
  case M_RX_CODE:
    PrintRTXCode(Output, radio->rx.codeType, radio->rx.code);
    break;
  case M_TX_CODE_TYPE:
    strncpy(Output, TX_CODE_TYPES[radio->tx.codeType], 31);
    break;
  case M_TX_CODE:
    PrintRTXCode(Output, radio->tx.codeType, radio->tx.code);
    break;
  case M_TX_OFFSET:
    // sprintf(Output, "%u.%05u", gVFO->offset / MHZ,
    //         gVFO->offset % MHZ);
    break;
  case M_TX_OFFSET_DIR:
    snprintf(Output, 15, TX_OFFSET_NAMES[gVFO->offsetDir]);
    break;
  case M_F_TXP:
    snprintf(Output, 15, TX_POWER_NAMES[gVFO->power]);
    break;
  default:
    break;
  }
}

void AcceptRadioConfig(const MenuItem *item, uint8_t subMenuIndex) {
  bool isVfo = true; // gCurrentApp == APP_VFO_CFG;
  switch (item->type) {
  case M_BW:
    // gCurrentPreset->band.bw = subMenuIndex;
    // BK4819_SetFilterBandwidth(subMenuIndex);
    // PRESETS_SaveCurrent();
    break;
  case M_F_TXP:
    gVFO->power = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_TX_OFFSET_DIR:
    gVFO->offsetDir = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_MODULATION:
    radio->modulation = subMenuIndex;
    RADIO_SaveCurrentVFO();
    RADIO_SetupByCurrentVFO();
    break;
  case M_STEP:
    gVFO->step = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_SQ_TYPE:
    gVFO->squelchType = subMenuIndex;
    BK4819_SquelchType(subMenuIndex);
    RADIO_SaveCurrentVFO();
    break;
  case M_SQ:
    gVFO->squelch = subMenuIndex;
    BK4819_Squelch(subMenuIndex, radio->rx.f, gSettings.sqlOpenTime,
                   gSettings.sqlCloseTime);
    RADIO_SaveCurrentVFO();
    break;

  case M_GAIN:
    gVFO->gainIndex = subMenuIndex;
    BK4819_SetAGC(RADIO_GetModulation() != MOD_AM,
                  gVFO->gainIndex);
    RADIO_SaveCurrentVFO();
    break;
  case M_TX:
    // gCurrentPreset->allowTx = subMenuIndex;
    // PRESETS_SaveCurrent();
    break;
  case M_RX_CODE_TYPE:
    radio->rx.codeType = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_RX_CODE:
    radio->rx.code = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_TX_CODE_TYPE:
    radio->tx.codeType = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_TX_CODE:
    radio->tx.code = subMenuIndex;
    RADIO_SaveCurrentVFO();
    break;
  case M_RADIO:
     if (isVfo) {
      radio->radio = subMenuIndex;
      RADIO_SaveCurrentVFO();
    }
    break;
  default:
    break;
  }
}

void OnRadioSubmenuChange(const MenuItem *item, uint8_t subMenuIndex) {
  switch (item->type) {
  case M_GAIN:
    RADIO_SetGain(subMenuIndex);
    break;
  case M_BW:
    /* gCurrentPreset->band.bw = subMenuIndex; */
    RADIO_SetupBandParams();
    break;
  default:
    break;
  }
}
