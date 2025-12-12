/*
 * cli_parse.c - CLI argument parsing utilities
 *
 * Part of bterm - a lightweight embedded CLI library.
 */

#include "cli_parse.h"
#include <stdlib.h>
#include <ctype.h>

/*
 * Check if a string contains any hexadecimal letter (A-F, a-f).
 */
static int contains_hex_letter(const char *str)
{
    for (const char *p = str; *p; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            return 1;
        }
    }
    return 0;
}

int cli_parse_u32(const char *str, uint32_t *result)
{
    if (str == NULL || result == NULL) {
        return -1;
    }

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return -1;
    }

    char *endptr;
    unsigned long val;
    int base;

    /* Determine base:
     * - 0x/0X prefix -> hex
     * - Contains A-F -> hex (for addresses like "A0030000")
     * - Otherwise -> decimal
     */
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
    } else if (contains_hex_letter(str)) {
        base = 16;
    } else {
        base = 10;
    }

    val = strtoul(str, &endptr, base);

    /* Skip trailing whitespace */
    while (*endptr && isspace((unsigned char)*endptr)) {
        endptr++;
    }

    /* Check if entire string was consumed */
    if (*endptr != '\0') {
        return -1;
    }

    *result = (uint32_t)val;
    return 0;
}

int cli_parse_u16(const char *str, uint16_t *result)
{
    uint32_t val;

    if (cli_parse_u32(str, &val) != 0) {
        return -1;
    }

    if (val > 0xFFFF) {
        return -1;
    }

    *result = (uint16_t)val;
    return 0;
}

int cli_parse_u8(const char *str, uint8_t *result)
{
    uint32_t val;

    if (cli_parse_u32(str, &val) != 0) {
        return -1;
    }

    if (val > 0xFF) {
        return -1;
    }

    *result = (uint8_t)val;
    return 0;
}

int cli_parse_i32(const char *str, int32_t *result)
{
    if (str == NULL || result == NULL) {
        return -1;
    }

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return -1;
    }

    /* Check for negative sign */
    int negative = 0;
    if (*str == '-') {
        negative = 1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Parse as unsigned */
    uint32_t uval;
    if (cli_parse_u32(str, &uval) != 0) {
        return -1;
    }

    /* Apply sign and check range */
    if (negative) {
        if (uval > (uint32_t)INT32_MAX + 1) {
            return -1;  /* Overflow */
        }
        *result = -(int32_t)uval;
    } else {
        if (uval > INT32_MAX) {
            return -1;  /* Overflow */
        }
        *result = (int32_t)uval;
    }

    return 0;
}
