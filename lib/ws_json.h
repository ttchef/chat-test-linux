
#ifndef WS_JSON_H
#define WS_JSON_H

#define WS_JSON_ERROR -1 
#define WS_JSON_OK 0

#define WS_JSON_MAX_KEY_SIZE 64 
#define WS_JSON_MAX_VALUE_SIZE 256
#define WS_JSON_OBJECT_MAX_FIELDS 16

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    WS_JSON_STRING,
    WS_JSON_NUMBER,
    WS_JSON_OBJECT,
} wsJsonType;

typedef struct wsJsonField {
    char key[WS_JSON_MAX_KEY_SIZE];
    wsJsonType type; 
    union {
        char stringValue[WS_JSON_MAX_VALUE_SIZE];
        double numberValue;
        struct {
            struct wsJsonField* children[WS_JSON_OBJECT_MAX_FIELDS];
            int32_t childCount;
        } object;
    };
} wsJson;

int32_t wsJsonAddString(wsJson* obj, const char* key, const char* val);
int32_t wsJsonAddNumber(wsJson* obj, const char* key, double val);
int32_t wsJsonCreateChild(wsJson* child, const char* key);
int32_t wsJsonAddChild(wsJson* obj, wsJson* child);
int32_t wsJsonToString(wsJson* obj, char* out, size_t size);

#endif
