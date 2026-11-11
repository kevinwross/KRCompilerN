import struct
import sys

INITRD_MAGIC = 0x4932334B
INITRD_VERSION = 1
INITRD_NAME_SIZE = 32
USER_CODE_BASE = 0x00400000
HEADER = struct.Struct("<III")
ENTRY = struct.Struct("<32sIII")

user_path, fault_path, wx_path, archive_path, assembly_path = sys.argv[1:6]
programs = [
    ("init", open(user_path, "rb").read()),
    ("fault", open(fault_path, "rb").read()),
    ("wx", open(wx_path, "rb").read()),
]
header = HEADER.pack(INITRD_MAGIC, INITRD_VERSION, len(programs))
payload_offset = HEADER.size + len(programs) * ENTRY.size
table = b""
payload = b""
for name, data in programs:
    encoded = name.encode("ascii").ljust(INITRD_NAME_SIZE, b"\0")
    table += ENTRY.pack(encoded, payload_offset + len(payload), len(data),
                        USER_CODE_BASE)
    payload += data
archive = header + table + payload
with open(archive_path, "wb") as handle:
    handle.write(archive)

lines = [".section .rodata", ".global __initrd_start", "__initrd_start:"]
for offset in range(0, len(archive), 16):
    chunk = archive[offset:offset + 16]
    lines.append(".byte " + ", ".join(f"0x{byte:02X}" for byte in chunk))
lines.extend([".global __initrd_end", "__initrd_end:"])
with open(assembly_path, "w", encoding="ascii") as handle:
    handle.write("\n".join(lines) + "\n")
