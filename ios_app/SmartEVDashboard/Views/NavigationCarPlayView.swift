import SwiftUI

/// Tab 4: Quản lý Dẫn đường Thông minh & Đồng bộ Apple CarPlay sang ESP32 OLED
struct NavigationCarPlayView: View {
    @ObservedObject var bleManager: ESP32BLEManager
    @State private var customIcon = "LEFT"
    @State private var customDistance = "200m"
    @State private var customStreet = "Đại lộ Nguyễn Huệ"
    @State private var showSentToast = false
    
    private let iconOptions = [
        ("LEFT", "⬅ Rẽ Trái"),
        ("RIGHT", "➡ Rẽ Phải"),
        ("STRAIGHT", "⬆ Đi Thẳng"),
        ("UTURN", "🔄 Quay Đầu"),
        ("DEST", "🏁 Đến Nơi")
    ]
    
    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Header Trạng thái Apple CarPlay
                carPlayStatusCard
                
                // Trình mô phỏng gửi chỉ dẫn nhanh
                quickNavSimulationCard
                
                // Gửi chỉ dẫn tùy chỉnh
                customNavInputCard
                
                // Hướng dẫn kết nối Apple CarPlay trên ô tô / màn hình xe
                carPlayGuideCard
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
            .padding(.bottom, 32)
        }
    }
    
    // MARK: - Thẻ Trạng thái CarPlay
    private var carPlayStatusCard: some View {
        HStack(spacing: 12) {
            Image(systemName: "applelogo")
                .font(.system(size: 28))
                .foregroundColor(.white)
            
            VStack(alignment: .leading, spacing: 2) {
                Text("APPLE CARPLAY COMPANION")
                    .font(.system(size: 13, weight: .bold, design: .monospaced))
                    .foregroundColor(.cyan)
                Text("Hỗ trợ chiếu giao diện Digital Cockpit & Dẫn đường trực tiếp")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.gray)
            }
            
            Spacer()
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.cyan.opacity(0.3), lineWidth: 1))
    }
    
    // MARK: - Chỉ dẫn nhanh
    private var quickNavSimulationCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: "location.north.line.fill")
                    .foregroundColor(.green)
                Text("MÔ PHỎNG DẪN ĐƯỜNG NHANH ➔ OLED XE")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
            }
            
            Divider().background(Color.gray.opacity(0.3))
            
            VStack(spacing: 10) {
                HStack(spacing: 10) {
                    Button(action: {
                        bleManager.sendNavigationUpdate(icon: "LEFT", distance: "150m", instruction: "Rẽ Trái - Lê Lợi")
                    }) {
                        HStack {
                            Image(systemName: "arrow.turn.up.left")
                            Text("Rẽ Trái (150m)")
                        }
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 10)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.blue.opacity(0.3)))
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.blue, lineWidth: 1))
                    }
                    
                    Button(action: {
                        bleManager.sendNavigationUpdate(icon: "RIGHT", distance: "300m", instruction: "Rẽ Phải - Hai Bà Trưng")
                    }) {
                        HStack {
                            Image(systemName: "arrow.turn.up.right")
                            Text("Rẽ Phải (300m)")
                        }
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 10)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.green.opacity(0.3)))
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.green, lineWidth: 1))
                    }
                }
                
                HStack(spacing: 10) {
                    Button(action: {
                        bleManager.sendNavigationUpdate(icon: "STRAIGHT", distance: "1.8km", instruction: "Đi Thẳng - Võ Văn Kiệt")
                    }) {
                        HStack {
                            Image(systemName: "arrow.up")
                            Text("Đi Thẳng (1.8km)")
                        }
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 10)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.cyan.opacity(0.3)))
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.cyan, lineWidth: 1))
                    }
                    
                    Button(action: {
                        bleManager.clearNavigation()
                    }) {
                        HStack {
                            Image(systemName: "xmark.octagon.fill")
                            Text("Xóa Màn Hình")
                        }
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(.red)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 10)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.red.opacity(0.15)))
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.red.opacity(0.6), lineWidth: 1))
                    }
                }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.white.opacity(0.08), lineWidth: 1))
    }
    
    // MARK: - Chỉ dẫn tùy chỉnh
    private var customNavInputCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: "slider.horizontal.3")
                    .foregroundColor(.yellow)
                Text("TÙY CHỈNH CHỈ DẪN DẪN ĐƯỜNG")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
            }
            
            Divider().background(Color.gray.opacity(0.3))
            
            // Icon Picker
            VStack(alignment: .leading, spacing: 6) {
                Text("Biểu tượng Mũi tên")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.gray)
                
                Picker("Mũi tên", selection: $customIcon) {
                    ForEach(iconOptions, id: \.0) { item in
                        Text(item.1).tag(item.0)
                    }
                }
                .pickerStyle(SegmentedPickerStyle())
            }
            
            // Distance Input
            VStack(alignment: .leading, spacing: 6) {
                Text("Khoảng cách")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.gray)
                
                TextField("Ví dụ: 250m hoặc 2.5km", text: $customDistance)
                    .font(.system(size: 13, weight: .bold, design: .monospaced))
                    .foregroundColor(.cyan)
                    .padding(10)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color.white.opacity(0.06)))
            }
            
            // Street Input
            VStack(alignment: .leading, spacing: 6) {
                Text("Tên đường / Hành động")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.gray)
                
                TextField("Ví dụ: Nguyễn Trãi", text: $customStreet)
                    .font(.system(size: 13, weight: .bold, design: .monospaced))
                    .foregroundColor(.cyan)
                    .padding(10)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color.white.opacity(0.06)))
            }
            
            Button(action: {
                bleManager.sendNavigationUpdate(icon: customIcon, distance: customDistance, instruction: customStreet)
                showSentToast = true
            }) {
                HStack {
                    Image(systemName: "paperplane.fill")
                    Text("GỬI CHỈ DẪN SANG ESP32 OLED")
                }
                .font(.system(size: 12, weight: .bold))
                .foregroundColor(.white)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 12)
                .background(RoundedRectangle(cornerRadius: 12).fill(LinearGradient(colors: [.cyan, .blue], startPoint: .leading, endPoint: .trailing)))
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.white.opacity(0.08), lineWidth: 1))
    }
    
    // MARK: - Hướng dẫn CarPlay
    private var carPlayGuideCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: "info.circle.fill")
                    .foregroundColor(.blue)
                Text("THÔNG TIN CARPLAY")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
            }
            
            Text("Khi cắm cáp hoặc kết nối CarPlay không dây trên ô tô/màn hình rời của xe, Smart EV Dashboard sẽ tự động hiển thị bảng đồng hồ kỹ thuật số và chỉ đường với giao diện tối ưu hóa cho màn hình cảm ứng ô tô.")
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(.gray)
                .lineSpacing(4)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.03)))
    }
}
