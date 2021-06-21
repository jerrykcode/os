#include "string.h"
#include "stddef.h"

void *memset(void *s_, uint8_t c, size_t n) {
    uint8_t *s = (uint8_t *)s_;
    while (n--) 
        *s++ = c;
    return s_;
}

void *memcpy(void *dest_, const void *src_, size_t n) {
    uint8_t *dest = (uint8_t *)dest_;
    uint8_t *src = (uint8_t *)src_;
    while (n--)
        *dest++ = *src++;
    return dest_;
}

int memcmp(const void *s1_, const void *s2_, size_t n) {
    uint8_t *s1 = (uint8_t *)s1_;
    uint8_t *s2 = (uint8_t *)s2_;
    int diff;
    while (n--) {
        diff = (int)(*s1++) - (int)(*s2++);
        if (diff)
            return diff;    
    }
    return 0;
}

char *strcpy(char *dest_, const char *src) {
    char *dest = dest_;
    while ((*dest++ = *src++));
    return dest_;
}

char *strncpy(char *dest_, const char *src, size_t n) {
    char *dest = dest_;
    while ((n--) && (*dest++ = *src++));
    while (n--) *dest++ = '\0';
    return dest_;
}

size_t strlen(const char *s) {
    size_t len;
    for (len = 0; *(s + len); len++);
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 < *s2 ? -1 : *s2 > *s1;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        n--;
        s1++;
        s2++;
    }
    if (!n)
        return 0;
    return *s1 < *s2 ? -1 : *s2 > *s1;
}

char *strchr(const char *s, uint8_t c) {
    while (*s && *s != c)
        s++;
    return *s == c ? s : NULL;
}

char *strrchr(const char *s, uint8_t c) {
    char *res = NULL;
    while (*s) {
        if (*s == c) res = s;
        s++;
    }
    return res;
}

char *strcat(char *dest_, const char *src) {
    char *dest = dest_;
    while (*dest++);
    dest--;
    while ((*dest++ = *src++));
    return dest_;
}

char *strncat(char *dest_, const char *src, size_t n) {
    char *dest = dest_;
    while (*dest++);
    dest--;
    while (n && (*dest++ = *src++))
        n--;
    if (!n)
        *dest = '\0';
    return dest_;
}
