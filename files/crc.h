#ifndef CRC_H
#define CRC_H

#include "common_types.h"

uint32 crc32(const char *filename);
uint32 crc32_syspath(const char *filename);

#endif
