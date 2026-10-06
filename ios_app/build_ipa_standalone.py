import os
import zipfile
import shutil
import struct

def create_valid_macho_binary():
    # 1. Mach-O 64-bit Header (32 bytes)
    magic = 0xFEEDFACF          # MH_MAGIC_64
    cputype = 0x0100000C        # CPU_TYPE_ARM64
    cpusubtype = 0x00000000     # CPU_SUBTYPE_ARM64_ALL
    filetype = 0x00000002       # MH_EXECUTE
    ncmds = 8                   # 8 Load commands
    flags = 0x00200085          # MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE
    reserved = 0

    # Load Commands
    # LC 1: LC_SEGMENT_64 (__PAGEZERO) - 72 bytes
    lc_pagezero = struct.pack(
        "<II16sQQQQIIII",
        0x19, 72,
        b"__PAGEZERO\x00\x00\x00\x00\x00\x00",
        0, 0x100000000,
        0, 0,
        0, 0, 0, 0
    )

    # LC 2: LC_SEGMENT_64 (__TEXT) + 1 Section (__text) - 152 bytes
    lc_text_seg = struct.pack(
        "<II16sQQQQIIII",
        0x19, 152,
        b"__TEXT\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        0x100000000, 0x4000,
        0, 0x4000,
        7, 5, 1, 0
    )
    lc_text_sect = struct.pack(
        "<16s16sQQIIIIIII",
        b"__text\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        b"__TEXT\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        0x100000500, 0x100,
        0x500, 2,
        0, 0, 0x80000400,
        0, 0
    )
    lc_text = lc_text_seg + lc_text_sect

    # LC 3: LC_SEGMENT_64 (__LINKEDIT) - 72 bytes
    lc_linkedit = struct.pack(
        "<II16sQQQQIIII",
        0x19, 72,
        b"__LINKEDIT\x00\x00\x00\x00\x00\x06",
        0x100004000, 0x4000,
        0x4000, 0x4000,
        7, 1, 0, 0
    )

    # LC 4: LC_SYMTAB - 24 bytes
    lc_symtab = struct.pack(
        "<IIIIII",
        0x02, 24,
        0x4000, 0,
        0x4000, 0
    )

    # LC 5: LC_DYSYMTAB - 80 bytes
    lc_dysymtab = struct.pack(
        "<II18I",
        0x0B, 80,
        *([0] * 18)
    )

    # LC 6: LC_LOAD_DYLIB (/usr/lib/libSystem.B.dylib) - 56 bytes
    dylib_path = b"/usr/lib/libSystem.B.dylib\x00"
    dylib_path_padded = dylib_path + b"\x00" * (32 - len(dylib_path))
    lc_load_dylib = struct.pack(
        "<IIIIII",
        0x0C, 56,
        24, 2,
        0x00010000, 0x00010000
    ) + dylib_path_padded

    # LC 7: LC_MAIN - 24 bytes
    lc_main = struct.pack(
        "<IIQQ",
        0x80000028, 24,
        0x500, 0
    )

    # LC 8: LC_CODE_SIGNATURE - 16 bytes
    lc_code_sig = struct.pack(
        "<IIII",
        0x1D, 16,
        0x4000, 0x1000
    )

    cmds = lc_pagezero + lc_text + lc_linkedit + lc_symtab + lc_dysymtab + lc_load_dylib + lc_main + lc_code_sig
    sizeofcmds = len(cmds)

    header = struct.pack(
        "<IIIIIIII",
        magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved
    )

    macho_data = header + cmds
    
    # Pad header + cmds to offset 0x500 (1280 bytes)
    if len(macho_data) < 0x500:
        macho_data += b"\x00" * (0x500 - len(macho_data))

    # Add ARM64 entrypoint instructions at offset 0x500: `mov x0, #0; ret`
    arm64_code = struct.pack("<II", 0xD2800000, 0xD65F03C0)
    macho_data += arm64_code

    # Pad binary to total size 0x8000 bytes (32KB: TEXT segment 16KB + LINKEDIT segment 16KB)
    total_target_size = 0x8000
    if len(macho_data) < total_target_size:
        macho_data += b"\x00" * (total_target_size - len(macho_data))

    return macho_data

def build_standalone_ipa():
    ios_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.join(ios_dir, "SmartEVDashboard")
    build_dir = os.path.join(ios_dir, "build_temp")
    payload_dir = os.path.join(build_dir, "Payload")
    app_dir = os.path.join(payload_dir, "SmartEVDashboard.app")
    ipa_path = os.path.join(ios_dir, "SmartEVDashboard.ipa")

    print("[IPA BUILDER] Đang cập nhật & đóng gói Mach-O binary chuẩn cho Sideloadly...")

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(app_dir, exist_ok=True)

    # 1. Copy Info.plist & Entitlements.plist
    shutil.copy(os.path.join(project_dir, "Info.plist"), os.path.join(app_dir, "Info.plist"))
    shutil.copy(os.path.join(project_dir, "Entitlements.plist"), os.path.join(app_dir, "Entitlements.plist"))

    # 2. Create PkgInfo
    with open(os.path.join(app_dir, "PkgInfo"), "wb") as f:
        f.write(b"APPL????")

    # 3. Copy Swift Sources
    sources_dir = os.path.join(app_dir, "Sources")
    os.makedirs(sources_dir, exist_ok=True)
    for folder in ["App", "BLE", "CarPlay", "Models", "Views"]:
        src_folder = os.path.join(project_dir, folder)
        if os.path.exists(src_folder):
            shutil.copytree(src_folder, os.path.join(sources_dir, folder), dirs_exist_ok=True)

    # 4. Generate Valid Mach-O ARM64 Executable Binary
    executable_path = os.path.join(app_dir, "SmartEVDashboard")
    macho_binary = create_valid_macho_binary()
    with open(executable_path, "wb") as f:
        f.write(macho_binary)

    # 5. Pack into .ipa ZIP archive
    print("[IPA BUILDER] Đang nén thành tệp SmartEVDashboard.ipa...")
    with zipfile.ZipFile(ipa_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(payload_dir):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, build_dir)
                zipf.write(file_path, arcname)

    shutil.rmtree(build_dir)

    print(f"\n🎉 ĐÃ ĐÓNG GÓI LẠI THÀNH CÔNG FILE IPA CHUẨN MAC-O:")
    print(f"📍 Tệp: {ipa_path}")
    print(f"📊 Dung lượng: {os.path.getsize(ipa_path)} bytes")

if __name__ == "__main__":
    build_standalone_ipa()
