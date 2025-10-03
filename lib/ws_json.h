
#ifndef WS_JSON_H
#define WS_JSON_H

#define WS_JSON_MAX_KEY_SIZE 64 
#define WS_JSON_MAX_VALUE_SIZE 256
#define WS_JSON_OBJECT_MAX_FIELDS 16

#include <stdint.h>

typedef struct {
    char key[WS_JSON_MAX_KEY_SIZE];
    char value[WS_JSON_MAX_VALUE_SIZE];
} wsJsonField;

typedef struct {
    wsJsonField fields[WS_JSON_OBJECT_MAX_FIELDS];
    int32_t count;
} wsJsonObject;

#endif
