#pragma once

/* Trap frame words consumed by trap_dispatch. */
#define TRAP_FRAME_PC 0U
#define TRAP_FRAME_FLAGS 1U
#define TRAP_USER_MODE_BIT 0x20U
#define USER_FAULT_STATUS 139
