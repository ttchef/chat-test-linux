
#include "ws_json.h"
#include "ws_globals.h"

wsJson* wsJsonInitObject(const char* key) {
    wsJson* obj = malloc(sizeof(wsJson));
    if (!obj) {
        WS_LOG_ERROR("Failed to allocate memory for json object: %s\n", key);
        return NULL;
    }
    memset(obj, 0, sizeof(wsJson));
    obj->type = WS_JSON_OBJECT;
    if (key) strncpy(obj->key, key, sizeof(obj->key) - 1);
    return obj;
}

wsJson* wsJsonInitString(const char* key, const char* val) {
    wsJson* obj = malloc(sizeof(wsJson));
    if (!obj) {
        WS_LOG_ERROR("Failed to allocate memory for json object: %s\n", key);
        return NULL;
    }
    memset(obj, 0, sizeof(wsJson));
    obj->type = WS_JSON_STRING;
    if (key) strncpy(obj->key, key, sizeof(obj->key) - 1);
    if (val) strncpy(obj->stringValue, val, sizeof(obj->stringValue) - 1);
    return obj;
}

wsJson* wsJsonInitNumber(const char* key, double val) {
    wsJson* obj = malloc(sizeof(wsJson));
    if (!obj) {
        WS_LOG_ERROR("Failed to allocate memory for json object: %s\n", key);
        return NULL;
    }
    memset(obj, 0, sizeof(wsJson));
    obj->type = WS_JSON_NUMBER;
    if (key) strncpy(obj->key, key, sizeof(obj->key) - 1);
    obj->numberValue = val;
    return obj;
}

void wsJsonAddChild(wsJson *parent, wsJson *child) {
    if (parent && parent->type == WS_JSON_OBJECT && parent->object.childCount < WS_JSON_OBJECT_MAX_FIELDS) {
        parent->object.children[parent->object.childCount++] = child;
    }
}

