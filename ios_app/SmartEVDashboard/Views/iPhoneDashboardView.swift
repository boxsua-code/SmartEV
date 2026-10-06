import SwiftUI

/// Màn hình chính Master Dashboard với thanh điều hướng 4 Tab phong cách Cyberpunk Glassmorphism
struct iPhoneDashboardView: View {
    @ObservedObject var bleManager: ESP32BLEManager
    @StateObject private var bmsManager = AntBmsBleManager()
    @State private var selectedTab = 0
    
    var body: some View {
        ZStack(alignment: .bottom) {
            // Background Dark Gradient
            LinearGradient(
                gradient: Gradient(colors: [Color(red: 0.04, green: 0.06, blue: 0.10), Color.black]),
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
            
            VStack(spacing: 0) {
                // Top App Branding
                topBrandingBar
                
                // Nội dung Tab chính
                TabView(selection: $selectedTab) {
                    CockpitView(bleManager: bleManager)
                        .tag(0)
                    
                    BmsPackView(bmsManager: bmsManager)
                        .tag(1)
                    
                    VotolSettingsView(bleManager: bleManager)
                        .tag(2)
                    
                    NavigationCarPlayView(bleManager: bleManager)
                        .tag(3)
                }
                .tabViewStyle(PageTabViewStyle(indexDisplayMode: .never))
                
                // Bottom Tab Bar
                customGlassTabBar
            }
        }
        .onAppear {
            // Tự động chuyển tiếp dữ liệu Pin ANT BMS sang ESP32 qua BLE
            bmsManager.onBmsDataSync = { bmsSyncStr in
                if bleManager.isConnected {
                    bleManager.sendCommand(bmsSyncStr)
                }
            }
        }
    }
    
    // MARK: - Top Branding Bar
    private var topBrandingBar: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("SMART EV DASHBOARD")
                    .font(.system(size: 14, weight: .black, design: .monospaced))
                    .foregroundColor(.cyan)
                Text("iOS Cockpit & Votol Controller")
                    .font(.system(size: 10, weight: .medium))
                    .foregroundColor(.gray)
            }
            
            Spacer()
            
            // Icon chỉ báo BLE ESP32 & ANT BMS
            HStack(spacing: 6) {
                HStack(spacing: 4) {
                    Circle()
                        .fill(bleManager.isConnected ? Color.green : Color.red)
                        .frame(width: 6, height: 6)
                    Text("ESP32")
                        .font(.system(size: 9, weight: .bold, design: .monospaced))
                        .foregroundColor(bleManager.isConnected ? .green : .gray)
                }
                .padding(.horizontal, 6)
                .padding(.vertical, 3)
                .background(Capsule().fill(Color.white.opacity(0.05)))
                
                HStack(spacing: 4) {
                    Circle()
                        .fill(bmsManager.isConnected ? Color.green : Color.red)
                        .frame(width: 6, height: 6)
                    Text("BMS")
                        .font(.system(size: 9, weight: .bold, design: .monospaced))
                        .foregroundColor(bmsManager.isConnected ? .green : .gray)
                }
                .padding(.horizontal, 6)
                .padding(.vertical, 3)
                .background(Capsule().fill(Color.white.opacity(0.05)))
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 6)
        .padding(.bottom, 6)
    }
    
    // MARK: - Custom Glass Tab Bar
    private var customGlassTabBar: some View {
        HStack {
            TabBarButton(icon: "speedometer", title: "Đồng Hồ", isSelected: selectedTab == 0) {
                withAnimation(.spring(response: 0.3, dampingFraction: 0.7)) {
                    selectedTab = 0
                }
            }
            
            TabBarButton(icon: "battery.100bolt", title: "Pin ANT", isSelected: selectedTab == 1) {
                withAnimation(.spring(response: 0.3, dampingFraction: 0.7)) {
                    selectedTab = 1
                }
            }
            
            TabBarButton(icon: "slider.horizontal.3", title: "Cài Đặt", isSelected: selectedTab == 2) {
                withAnimation(.spring(response: 0.3, dampingFraction: 0.7)) {
                    selectedTab = 2
                }
            }
            
            TabBarButton(icon: "location.north.circle.fill", title: "Dẫn Đường", isSelected: selectedTab == 3) {
                withAnimation(.spring(response: 0.3, dampingFraction: 0.7)) {
                    selectedTab = 3
                }
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(
            RoundedRectangle(cornerRadius: 24)
                .fill(Color(red: 0.08, green: 0.10, blue: 0.16).opacity(0.95))
                .overlay(RoundedRectangle(cornerRadius: 24).stroke(Color.white.opacity(0.12), lineWidth: 1))
                .shadow(color: Color.cyan.opacity(0.15), radius: 10, y: -2)
        )
        .padding(.horizontal, 16)
        .padding(.bottom, 10)
    }
}

// MARK: - Tab Bar Button Component
struct TabBarButton: View {
    let icon: String
    let title: String
    let isSelected: Bool
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 4) {
                Image(systemName: icon)
                    .font(.system(size: 18, weight: isSelected ? .bold : .regular))
                    .foregroundColor(isSelected ? .cyan : .gray)
                    .scaleEffect(isSelected ? 1.15 : 1.0)
                
                Text(title)
                    .font(.system(size: 10, weight: isSelected ? .bold : .medium))
                    .foregroundColor(isSelected ? .white : .gray)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 4)
        }
    }
}
