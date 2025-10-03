
#ifndef WS_GLOBALS_H
#define WS_GLOBALS_H

#define WS_ERROR -1
#define WS_OK 0

#define WS_BUFFER_SIZE 4096

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <string.h> 
#include <poll.h> 
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

// LOGGING
#ifdef WS_ENABLE_LOG_DEBUG
    #define WS_LOG_DEBUG(msg, ...) \
        do { \
            fprintf(stdout, "[WS DEBUG] "); \
            fprintf(stdout, msg, ##__VA_ARGS__); \
        } while(0) 
#else 
    #define WS_LOG_DEBUG(msg, ...) 
#endif

#ifdef WS_ENABLE_LOG_ERROR
    #define WS_LOG_ERROR(msg, ...) \
        do { \
            fprintf(stderr, "[WS ERROR] "); \
            fprintf(stderr, msg, ##__VA_ARGS__); \
        } while(0)
#else 
    #define WS_LOG_ERROR(msg, ...)
#endif


#endif
