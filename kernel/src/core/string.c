#include <string.h>
#include <stdarg.h>

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* ptr = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return ptr;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* ptr = dest;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n--) {
        *dest++ = '\0';
    }
    return ptr;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

int strncmp(const char* str1, const char* str2, size_t n) {
    while (n && *str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *str1 - *str2;
}

static int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

int strcasecmp(const char* str1, const char* str2) {
    while (*str1 && *str2) {
        int diff = tolower(*str1) - tolower(*str2);
        if (diff != 0) {
            return diff;
        }
        str1++;
        str2++;
    }
    return tolower(*str1) - tolower(*str2);
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest;
    while (*dest) {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return ptr;
}

char* strchr(const char* str, int c) {
    while (*str && *str != (char)c) {
        str++;
    }
    if (*str == (char)c) {
        return (char*)str;
    }
    return NULL;
}

char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == (char)c) {
            last = str;
        }
        str++;
    }
    return (char*)last;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (num--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        while (num--) {
            *d++ = *s++;
        }
    } else {
        d += num;
        s += num;
        while (num--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    
    while (num--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

char* itoa(int value, char* str, int base) {
    static char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    char* ptr = str;
    char* first = str;
    int negative = 0;
    
    if (value < 0 && base == 10) {
        negative = 1;
        value = -value;
    }
    
    do {
        *ptr++ = digits[value % base];
        value /= base;
    } while (value != 0);
    
    if (negative) {
        *ptr++ = '-';
    }
    
    *ptr = '\0';
    
    char* end = ptr - 1;
    while (first < end) {
        char temp = *first;
        *first++ = *end;
        *end-- = temp;
    }
    
    return str;
}

char* strtok(char* str, const char* delim) {
    static char* saveptr = NULL;
    
    if (str != NULL) {
        saveptr = str;
    } else {
        if (saveptr == NULL) {
            return NULL;
        }
        str = saveptr;
    }
    
    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) {
            if (*str == *d) {
                found = 1;
                break;
            }
            d++;
        }
        
        if (!found) {
            break;
        }
        str++;
    }
    
    if (*str == '\0') {
        saveptr = NULL;
        return NULL;
    }
    
    char* token = str;
    
    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) {
            if (*str == *d) {
                found = 1;
                break;
            }
            d++;
        }
        
        if (found) {
            *str = '\0';
            saveptr = str + 1;
            return token;
        }
        str++;
    }
    
    saveptr = NULL;
    return token;
}

static int sprintf_num(char* buf, int num, int base, int width) {
    char digits[] = "0123456789ABCDEF";
    char tmp[32];
    int i = 0;
    int negative = 0;
    
    if (num < 0 && base == 10) {
        negative = 1;
        num = -num;
    }
    
    do {
        tmp[i++] = digits[num % base];
        num /= base;
    } while (num != 0);
    
    if (negative) {
        tmp[i++] = '-';
    }
    
    while (i < width) {
        tmp[i++] = ' ';
    }
    
    int len = i;
    while (i > 0) {
        *buf++ = tmp[--i];
    }
    
    return len;
}

int sprintf(char* str, const char* format, ...) {
    char* buf = str;
    const char* fmt = format;
    va_list args;
    va_start(args, format);
    
    while (*fmt) {
        if (*fmt != '%') {
            *buf++ = *fmt++;
            continue;
        }
        
        fmt++;
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        
        switch (*fmt) {
            case 'd':
                buf += sprintf_num(buf, va_arg(args, int), 10, width);
                break;
            case 'x':
            case 'X':
                buf += sprintf_num(buf, va_arg(args, int), 16, width);
                break;
            case 's':
            {
                char* s = va_arg(args, char*);
                while (*s) {
                    *buf++ = *s++;
                }
                break;
            }
            case 'c':
                *buf++ = (char)va_arg(args, int);
                break;
            case '%':
                *buf++ = '%';
                break;
            default:
                *buf++ = *fmt;
                break;
        }
        fmt++;
    }
    
    *buf = '\0';
    return buf - str;
}