import os
import zipfile

def package_ios_project():
    source_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.join(source_dir, "SmartEVDashboard")
    output_zip = os.path.join(source_dir, "SmartEVDashboard-iOS-IPA-Ready.zip")
    
    print(f"[IOS PACKAGER] Đang đóng gói toàn bộ dự án Xcode iOS...")
    
    with zipfile.ZipFile(output_zip, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(project_dir):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, source_dir)
                zipf.write(file_path, arcname)
                print(f"  + Added: {arcname}")
                
    print(f"\n✅ Đã tạo thành công gói dự án sẵn sàng xuất IPA:")
    print(f"📦 File: {output_zip}")

if __name__ == "__main__":
    package_ios_project()
