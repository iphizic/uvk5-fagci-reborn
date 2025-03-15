#include "mem_manager.h"
#include "../driver/eeprom.h"
#include "../settings.h"
#include <string.h>

#define DEF_READ(type) type MEMMANAGER_read_##type (uint32_t addr) { \
  type result; \
  MemBindata data = MEMMANAGER_getBindata(addr); \
  memcpy(&result, data.data , data.length); \
  return result; \
}

static MapItem eepromMap[EEPROM_MAP_SIZE];

void MEMMANAGER_init(){
  uint32_t magic_eeprom = 0;
  EEPROM_ReadBuffer(0,&magic_eeprom, sizeof(magic_eeprom));
  if (magic_eeprom != MAGIC_MEMORY_VALUE){
    MEMMANAGER_format();
  };

  uint8_t slot = 0;
  uint16_t size = 0;
  uint32_t addr = sizeof(magic_eeprom);
  bool exit = true;

  EEPROM_ReadBuffer(addr, &slot, sizeof(slot));
  addr += sizeof(slot);

  if (slot != SETTINGS_SLOT){
    MEMMANAGER_format();
  }
  eepromMap[0] = (MapItem){
                 .slot = slot,
                 .addres = addr};

  SETTINGS_Load();

};

Options MEMMANAGER_getOptions(uint16_t num){
  MapItem item = eepromMap[num];
  Options opt;
  EEPROM_ReadBuffer(item.addres, &opt, OPTIONS_SIZE);
  return opt;
};

MemBindata MEMMANAGER_getBindata(uint32_t addr){
  MemBindata data;
  EEPROM_ReadBuffer(addr, &data.type, sizeof(data.type));
  EEPROM_ReadBuffer(addr + sizeof(data.type), &data.length, sizeof(data.length));
  EEPROM_ReadBuffer(addr + sizeof(data.type) + sizeof(data.length), &data.data, data.length);
  return data;
}

void MEMMANAGER_read(uint32_t addr, void *obj){
  MemBindata data = MEMMANAGER_getBindata(addr);
  memcpy(obj, data.data , data.length);
};

DEF_READ(uint8_t);
DEF_READ(uint16_t);
DEF_READ(uint32_t);
DEF_READ(bool);


