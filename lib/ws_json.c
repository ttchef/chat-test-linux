
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

int32_t wsJsonToString(wsJson *obj, char *out, size_t size) {
    if (!obj) {
        WS_LOG_ERROR("Input json obj is NULL\n");
        return WS_ERROR;
    }
    if (!out) {
        WS_LOG_ERROR("Input buffer for output is NULL\n");
        return WS_ERROR;
    }
    
    size_t used = 0;

    switch (obj->type) {
        case WS_JSON_STRING:
            used += snprintf(out + used, size - used, "\"%s\"", obj->stringValue);
            break;
        case WS_JSON_NUMBER:
            used += snprintf(out + used, size - used, "%g", obj->numberValue);
            break;
        case WS_JSON_OBJECT:
            used += snprintf(out + used, size - used, "{");
#ifdef WS_JSON_ADD_NEW_LINE
            used += snprintf(out + used, size - used, "\n");
#endif
            for (int32_t i = 0; i < obj->object.childCount; i++) {
                wsJson* child = obj->object.children[i];
                if (i > 0) {
                    used += snprintf(out + used, size - used, ",");
#ifdef WS_JSON_ADD_NEW_LINE
                    used += snprintf(out + used, size - used, "\n");
#endif
                }
                used += snprintf(out + used, size - used, "\"%s\": ", child->key);
                wsJsonToString(child, out + used, size - used);
                used = strlen(out);
            }
            used += snprintf(out + used, size - used, "}");
            break;
        default:
            WS_LOG_ERROR("Failed to parse json into string\n");
            return WS_ERROR;
    }

    return WS_OK;
}


wsJson* wsJsonGet(wsJson* obj, const char* key) {
    if (!obj || !key) {
        WS_LOG_ERROR("Invalid input is NULL\n");
        return NULL;
    } 
    if (obj->type != WS_JSON_OBJECT) {
        WS_LOG_ERROR("Obj is not from type WS_JSON_OBJECT\n");
        return NULL;
    }

    for (int32_t i = 0; i < obj->object.childCount; i++) {
        wsJson* child = obj->object.children[i];
        if (child->key && strcmp(child->key, key) == 0) {
            return child;
        }
    }

    return NULL;
}

const char* wsJsonGetString(wsJson* obj, const char* key) {
    wsJson* child = wsJsonGet(obj, key);
    if (child && child->type == WS_JSON_STRING) {
        return child->stringValue;
    }
    return NULL;
}

double wsJsonGetNumber(wsJson *obj, const char *key) {
    wsJson* child = wsJsonGet(obj, key);
    if (child && child->type == WS_JSON_NUMBER) {
        return child->numberValue;
    }
    return -1;
}

void wsJsonFree(wsJson *obj) {
    if (!obj) {
        WS_LOG_ERROR("JSON obj is NULL on free!\n");
        return;
    }
    if (obj->type == WS_JSON_OBJECT) {
        for (int32_t i = 0; i < obj->object.childCount; i++) {
            wsJsonFree(obj->object.children[i]);
        }
    }
    free(obj);
}

