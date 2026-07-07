#ifndef STRING_H
#define STRING_H

#include <types.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
int strcasecmp(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
char* itoa(int value, char* str, int base);
int sprintf(char* str, const char* format, ...);
char* strtok(char* str, const char* delim);

#endif