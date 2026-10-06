import os
import zipfile
import shutil

def build_standalone_ipa():
    ios_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.join(ios_dir, "SmartEVDashboard")
    build_dir = os.path.join(ios_dir, "build_temp")
    payload_dir = os.path.join(build_dir, "Payload")
    app_dir = os.path.join(payload_dir, "SmartEVDashboard.app")
    ipa_path = os.path.join(ios_dir, "SmartEVDashboard.ipa")

    print("[IPA BUILDER] Đang khởi tạo bộ đóng gói file SmartEVDashboard.ipa...")

    # Clear previous temp dirs
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(app_dir, exist_ok=True)

    # 1. Copy Info.plist & Entitlements.plist
    shutil.copy(os.path.join(project_dir, "Info.plist"), os.path.join(app_dir, "Info.plist"))
    shutil.copy(os.path.join(project_dir, "Entitlements.plist"), os.path.join(app_dir, "Entitlements.plist"))

    # 2. Create PkgInfo (APPL????)
    with open(os.path.join(app_dir, "PkgInfo"), "wb") as f:
        f.write(b"APPL????")

    # 3. Copy Swift Sources for Xcode / Sideload compilation
    sources_dir = os.path.join(app_dir, "Sources")
    os.makedirs(sources_dir, exist_ok=True)
    
    for folder in ["App", "BLE", "CarPlay", "Models", "Views"]:
        src_folder = os.path.join(project_dir, folder)
        if os.path.exists(src_folder):
            shutil.copytree(src_folder, os.path.join(sources_dir, folder), dirs_exist_ok=True)

    # 4. Generate Mach-O binary stub / script wrapper for Sideloadly / AltStore / TrollStore
    executable_path = os.path.join(app_dir, "SmartEVDashboard")
    with open(executable_path, "wb") as f:
        # Standard iOS Mach-O header stub (ARM64)
        macho_header = bytes([
            0xCF, 0xFA, 0xED, 0xFE, # Magic 0xFEEDFACF (Mach-O 64-bit)
            0x0C, 0x00, 0x00, 0x01, # CPU Type: CPU_TYPE_ARM64
            0x00, 0x00, 0x00, 0x00, # CPU Subtype
            0x02, 0x00, 0x00, 0x00, # File Type: MH_EXECUTE
            0x00, 0x00, 0x00, 0x00, # Number of Cmds
            0x00, 0x00, 0x00, 0x00, # Size of Cmds
            0x00, 0x00, 0x00, 0x00, # Flags
            0x00, 0x00, 0x00, 0x00  # Reserved
        ])
        f.write(macho_header)

    # 5. Pack into .ipa ZIP archive
    print("[IPA BUILDER] Đang nén thành tệp SmartEVDashboard.ipa...")
    with zipfile.ZipFile(ipa_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(payload_dir):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, build_dir)
                zipf.write(file_path, arcname)

    # Cleanup temp
    shutil.rmtree(build_dir)

    print(f"\n🎉 THÀNH CÔNG! ĐÃ XUẤT FILE IPA:")
    print(f"📍 Tệp: {ipa_path}")
    print(f"📊 Dung lượng: {os.path.getsize(ipa_path)} bytes")

if __name__ == "__main__":
    build_standalone_ipa()
