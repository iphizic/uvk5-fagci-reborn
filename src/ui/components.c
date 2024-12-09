#include "components.h"
#include "../driver/st7565.h"
#include "../helper/measurements.h"
// #include "../helper/presetlist.h"
#include "graphics.h"
#include <stdint.h>

void UI_Battery(uint8_t Level) {
  DrawRect(LCD_WIDTH - 13, 0, 12, 5, C_FILL);
  FillRect(LCD_WIDTH - 12, 1, Level, 3, C_FILL);
  DrawVLine(LCD_WIDTH - 1, 1, 3, C_FILL);

  if (Level > 10) {
    DrawHLine(LCD_WIDTH - 1 - 3, 1, 3, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 7, 1, 5, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 3, 3, 3, C_INVERT);
  }
}

void UI_RSSIBar(uint16_t rssi, int8_t s, uint32_t f, uint8_t y) {
  if (rssi == 0)
    return;
  const uint8_t BAR_LEFT_MARGIN = 32;
  const uint8_t BAR_BASE = y + 7;

  int dBm = Rssi2DBm(rssi);
  // uint8_t s = DBm2S(dBm, f >= 3000000);

  FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);

  for (uint8_t i = 0; i < s; ++i) {
    /* if (i >= 9) {
      FillRect(BAR_LEFT_MARGIN + i * 5, y + 2, 4, 6, C_FILL);
    } else { */
    FillRect(BAR_LEFT_MARGIN + i * 5, y + 3, 4, 4, C_FILL);
    // }
  }

  PrintMediumEx(LCD_WIDTH - 1, BAR_BASE, 2, true, "%d", dBm);
  if (s < 10) {
    PrintMedium(0, BAR_BASE, "S%u", s);
  } else {
    PrintMedium(0, BAR_BASE, "S9+%u0", s - 9);
  }
}

void UI_FSmall(uint32_t f) {
  PrintSmallEx(LCD_WIDTH - 1, 15, 2, true,
               modulationTypeOptions[RADIO_GetModulation()]);
  // PrintSmallEx(LCD_WIDTH - 1, 21, 2, true,
  //              RADIO_GetBWName(gCurrentPreset->band.bw));

  // uint16_t step = StepFrequencyTable[gCurrentPreset->band.step];

  // PrintSmall(0, 21, "%u.%02uk", step / 100, step % 100);

  /* if (gSettings.upconverter) {
    UI_FSmallest(radio->rx.f, 32, 21);
  } */

  PrintSmallEx(32, 21, POS_C, C_FILL, "S:%u", RADIO_GetSNR());
  PrintSmallEx(52, 21, POS_C, C_FILL, "N:%u", BK4819_GetNoise());
  PrintSmallEx(72, 21, POS_C, C_FILL, "G:%u", BK4819_GetGlitch());
  // PrintSmallEx(92, 21, POS_C, C_FILL, "SQ:%u", gCurrentPreset->band.squelch);

  PrintMediumEx(64, 15, POS_C, C_FILL, "%u.%05u", f / MHZ, f % MHZ);
}

void UI_FSmallest(uint32_t f, uint8_t x, uint8_t y) {
  PrintSmall(x, y, "%u.%05u", f / MHZ, f % MHZ);
}

void drawTicks(uint8_t y, uint32_t fs, uint32_t fe, uint32_t div, uint8_t h) {
  for (uint32_t f = fs - (fs % div) + div; f < fe; f += div) {
    uint8_t x = ConvertDomain(f, fs, fe, 0, LCD_WIDTH - 1);
    DrawVLine(x, y, h, C_FILL);
  }
}

void UI_DrawTicks(uint8_t y) {
  // const FRange *range = &band->bounds;
  // uint32_t fs = range->start;
  // uint32_t fe = range->end;
  // uint32_t bw = fe - fs;
  //
  // for (uint32_t p = 100000000; p >= 10; p /= 10) {
  //   if (p < bw) {
  //     drawTicks(y, fs, fe, p / 2, 2);
  //     drawTicks(y, fs, fe, p, 3);
  //     return;
  //   }
  // }
}

void UI_DrawSpectrumElements(const uint8_t sy, uint8_t msmDelay, int16_t sq) {
  PrintSmallEx(0, sy - 3, POS_L, C_FILL, "%ums", msmDelay);
  if (sq >= 255) {
    PrintSmallEx(LCD_WIDTH - 2, sy - 3, POS_R, C_FILL, "SQ off");
  } else {
    PrintSmallEx(LCD_WIDTH - 2, sy - 3, POS_R, C_FILL, "SQ %d", sq);
  }
  // PrintSmallEx(LCD_WIDTH - 2, sy - 3 + 6, POS_R, C_FILL, "%s",
  //              modulationTypeOptions[currentBand->modulation]);

  // if (gLastActiveLoot) {
  //   PrintMediumBoldEx(LCD_XCENTER, 14, POS_C, C_FILL, "%u.%05u",
  //                     gLastActiveLoot->f / MHZ, gLastActiveLoot->f % MHZ);
  // }

  // uint32_t fs = currentBand->bounds.start;
  // uint32_t fe = currentBand->bounds.end;

  // PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%u.%05u", fs / MHZ, fs % MHZ);
  // PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u.%05u", fe / MHZ,
  //              fe % MHZ);
}

void UI_ShowWait() {
  FillRect(0, 32 - 5, 128, 9, C_FILL);
  PrintMediumBoldEx(64, 32 + 2, POS_C, C_CLEAR, "WAIT");
  ST7565_Blit();
}
