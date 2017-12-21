#include <Arduino.h>
#include <SD.h>

bool ihex_open(const char *filename);
bool ihex_read(uint32_t addr, uint8_t *data, uint32_t len, uint32_t *count = nullptr);
bool ihex_end(void);
void ihex_close(void);

