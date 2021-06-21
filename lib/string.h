#ifndef LIB_STRING_H__
#define LIB_STRING_H__
#include "stdint.h"
void *memset(void *s, uint8_t c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, uint8_t c);
char *strrchr(const char *s, uint8_t c);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
#endif
