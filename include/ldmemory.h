/*!
 * @file ldmemory.h
 * @brief Public API. Operations for managing memory.
 */

#pragma once

#include "ldexport.h"

LD_EXPORT(void *) LDAlloc(const size_t bytes);
LD_EXPORT(void) LDFree(void *const buffer);
LD_EXPORT(char *) LDStrDup(const char *const string);
LD_EXPORT(void *) LDRealloc(void *const buffer, const size_t bytes);
LD_EXPORT(void *) LDCalloc(const size_t nmemb, const size_t size);
LD_EXPORT(char *) LDStrNDup(const char *const str, const size_t n);

LD_EXPORT(void) LDSetMemoryRoutines(void *(*const newMalloc)(const size_t),
    void (*const newFree)(void *const),
    void *(*const newRealloc)(void *const, const size_t),
    char *(*const newStrDup)(const char *const),
    void *(*const newCalloc)(const size_t, const size_t),
    char *(*const newStrNDup)(const char *const, const size_t));

LD_EXPORT(void) LDGlobalInit();
