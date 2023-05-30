/*
 * AVR loader helpers
 *
 * Copyright (c) 2019-2020 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "hw/loader.h"
#include "elf.h"
#include "boot.h"
#include "qemu/error-report.h"

// static const char *avr_elf_e_flags_to_cpu_type(uint32_t flags)
// {
//     switch (flags & EF_AVR_MACH) {
//     case bfd_mach_avr1:
//         return AVR_CPU_TYPE_NAME("avr1");
//     case bfd_mach_avr2:
//         return AVR_CPU_TYPE_NAME("avr2");
//     case bfd_mach_avr25:
//         return AVR_CPU_TYPE_NAME("avr25");
//     case bfd_mach_avr3:
//         return AVR_CPU_TYPE_NAME("avr3");
//     case bfd_mach_avr31:
//         return AVR_CPU_TYPE_NAME("avr31");
//     case bfd_mach_avr35:
//         return AVR_CPU_TYPE_NAME("avr35");
//     case bfd_mach_avr4:
//         return AVR_CPU_TYPE_NAME("avr4");
//     case bfd_mach_avr5:
//         return AVR_CPU_TYPE_NAME("avr5");
//     case bfd_mach_avr51:
//         return AVR_CPU_TYPE_NAME("avr51");
//     case bfd_mach_avr6:
//         return AVR_CPU_TYPE_NAME("avr6");
//     case bfd_mach_avrtiny:
//         return AVR_CPU_TYPE_NAME("avrtiny");
//     case bfd_mach_avrxmega2:
//         return AVR_CPU_TYPE_NAME("xmega2");
//     case bfd_mach_avrxmega3:
//         return AVR_CPU_TYPE_NAME("xmega3");
//     case bfd_mach_avrxmega4:
//         return AVR_CPU_TYPE_NAME("xmega4");
//     case bfd_mach_avrxmega5:
//         return AVR_CPU_TYPE_NAME("xmega5");
//     case bfd_mach_avrxmega6:
//         return AVR_CPU_TYPE_NAME("xmega6");
//     case bfd_mach_avrxmega7:
//         return AVR_CPU_TYPE_NAME("xmega7");
//     default:
//         return NULL;
//     }
// }

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

typedef struct {
    char signature[4];
    byte prg_block_count;
    byte chr_block_count;
    word rom_type;
    byte reserved[8];
} ines_header;

static ines_header fce_rom_header;
static char rom[1048576];
static void romread(char *rom, void *buf, int size)
{
    static int off = 0;
    memcpy(buf, rom + off, size);
    off += size;
}


static int fce_load_rom(char *rom)
{
    romread(rom, &fce_rom_header, sizeof(fce_rom_header));

    if (memcmp(fce_rom_header.signature, "NES\x1A", 4)) {
        return -1;
    }
    return 0;
}

bool nes6502_load_firmware(AVRCPU *cpu, MachineState *ms,
                       MemoryRegion *program_mr, const char *firmware)
{
    g_autofree char *filename = NULL;
    // int bytes_loaded;
    // uint64_t entry;
    // uint32_t e_flags;

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, firmware);
    if (filename == NULL) {
        error_report("Unable to find %s", firmware);
        return false;
    }

    FILE *fp = fopen(firmware, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Open rom file failed.\n");
        exit(1);
    }
    int nread = fread(rom, sizeof(rom), 1, fp);
    if (nread == 0 && ferror(fp)) {
        fprintf(stderr, "Read rom file failed.\n");
        exit(1);
    }
    if (fce_load_rom(rom) != 0)
    {
        fprintf(stderr, "Invalid or unsupported rom.\n");
        exit(1);
    }
    // signal(SIGINT, do_exit);
    return true;
}
