
#ifndef WS_JSON_H
#define WS_JSON_H

#include "ws_globals.h"

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

typedef struct wsJson {
    char key[WS_JSON_MAX_KEY_SIZE];
    wsJsonType type; 
    union {
        char stringValue[WS_JSON_MAX_VALUE_SIZE];
        double numberValue;
        struct {
            struct wsJson* children[WS_JSON_OBJECT_MAX_FIELDS];
            int32_t childCount;
        } object;
    };
} wsJson;

// Create Notes
wsJson* wsJsonInitChild(const char* key);
wsJson* wsJsonInitString(const char* key, const char* val);
wsJson* wsJsonInitNumber(const char* key, double val);

// Adds a new child to the json object
void wsJsonAddField(wsJson* parent, wsJson* child);
int32_t wsJsonToString(wsJson* obj, char* out, size_t size);
wsJson* wsJsonGet(wsJson* obj, const char* key);
const char* wsJsonGetString(wsJson* obj, const char* key);
double wsJsonGetNumber(wsJson* obj, const char* key);

// Goes recursive trough the json tree and frees everything
void wsJsonFree(wsJson* obj);

#endif
