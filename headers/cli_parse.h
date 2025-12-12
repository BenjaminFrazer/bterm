/*
 * cli_parse.h - CLI argument parsing utilities
 *
 * Provides robust numeric parsing that handles:
 *   - Decimal: "123", "0"
 *   - Hex with prefix: "0xABCD", "0X1a2b"
 *   - Hex without prefix: "ABCD", "a0030000" (auto-detected if contains A-F)
 *   - Error detection for invalid input
 *
 * Part of bterm - a lightweight embedded CLI library.
 */

#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include <stdint.h>

/*
 * Parse a string as an unsigned 32-bit integer.
 *
 * Automatically detects base:
 *   - "0x..." or "0X..." -> hexadecimal
 *   - Contains A-F/a-f   -> hexadecimal (e.g., "A0030000")
 *   - Otherwise          -> decimal
 *
 * @param str    Input string to parse (NULL-terminated)
 * @param result Pointer to store parsed value
 * @return 0 on success, -1 on error (NULL, empty, invalid chars)
 */
int cli_parse_u32(const char *str, uint32_t *result);

/*
 * Parse a string as an unsigned 16-bit integer.
 * Same base detection as cli_parse_u32.
 *
 * @return 0 on success, -1 on error or if value exceeds 0xFFFF
 */
int cli_parse_u16(const char *str, uint16_t *result);

/*
 * Parse a string as an unsigned 8-bit integer.
 * Same base detection as cli_parse_u32.
 *
 * @return 0 on success, -1 on error or if value exceeds 0xFF
 */
int cli_parse_u8(const char *str, uint8_t *result);

/*
 * Parse a string as a signed 32-bit integer.
 * Handles negative values: "-123", "-0x10"
 *
 * @return 0 on success, -1 on error
 */
int cli_parse_i32(const char *str, int32_t *result);

#endif /* CLI_PARSE_H */
