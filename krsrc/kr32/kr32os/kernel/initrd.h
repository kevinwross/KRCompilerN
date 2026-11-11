#pragma once

#define INITRD_MAGIC 0x4932334BU
#define INITRD_VERSION 1U
#define INITRD_NAME_SIZE 32U
#define INITRD_MAX_ENTRIES 64U

/*
 * K32I initrd on-disk layout.
 *
 * All fields are little-endian 32-bit values, matching the KR32 target.
 * The archive is:
 *
 *   initrd_header
 *   initrd_entry[count]
 *   payload bytes
 *
 * Entry offsets are relative to the start of the archive. Names are
 * NUL-padded to 32 bytes.
 */
struct initrd_header {
    unsigned int magic;
    unsigned int version;
    unsigned int count;
};

typedef char initrd_name_t[32]; /* INITRD_NAME_SIZE */

struct initrd_entry {
    initrd_name_t name;
    unsigned int offset;
    unsigned int size;
    unsigned int entry;
};
