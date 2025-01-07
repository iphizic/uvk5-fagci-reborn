#include "../config.h"
#include "../settings.h"

#define PROT_DEF_READ(type) type MEMMANAGER_read_##type (uint32_t addr);

typedef struct {
  uint8_t type;
  uint16_t length;
  uint8_t data[MAX_BINDATA_LENGTH];
} MemBindata;

typedef struct {
  uint8_t option;
  uint32_t address;
} Option;

typedef struct {
  uint8_t type;
  uint32_t address;
  Option options[MAX_OPTIONS_LENGTH];
} Options;

typedef struct  {
  uint8_t slot;
  uint32_t addres;
} MapItem;

enum{
  EMPTY_SLOT = 0,
  OPTION_SLOT = 1,
  SETTINGS_SLOT = 2,
  VFO_SLOT = 3,
  SSB_PATCH_SLOT = 4,
};

#define OPTIONS_SIZE sizeof(Options)

void MEMMANAGER_init();
void MEMMANAGER_format();
Options MEMMANAGER_getOptions(uint16_t num);
MemBindata MEMMANAGER_getBindata(uint32_t addr);

PROT_DEF_READ(uint8_t);
PROT_DEF_READ(uint16_t);
PROT_DEF_READ(uint32_t);
PROT_DEF_READ(bool);

