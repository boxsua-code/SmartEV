import UIKit
import CarPlay
import Combine

/// CarPlay Scene Delegate handling connection to Apple CarPlay head unit display
class CarPlaySceneDelegate: UIResponder, CPTemplateApplicationSceneDelegate {
    
    var interfaceController: CPInterfaceController?
    var cancellables = Set<AnyCancellable>()
    
    // MARK: - CarPlay Connection Lifecycle
    func templateApplicationScene(_ templateApplicationScene: CPTemplateApplicationScene, didConnect interfaceController: CPInterfaceController) {
        self.interfaceController = interfaceController
        print("[Apple CarPlay] ✅ Đã kết nối với màn hình Apple CarPlay trên ô tô/xe điện!")
        
        let bleManager = (UIApplication.shared.delegate as? AppDelegate)?.bleManager ?? ESP32BLEManager()
        
        // Cấu hình CarPlay Templates
        CarPlayManager.shared.setup(interfaceController: interfaceController, bleManager: bleManager)
        
        // Lắng nghe thay đổi Telemetry từ ESP32 để cập nhật giao diện CarPlay
        bleManager.$telemetry
            .receive(on: DispatchQueue.main)
            .sink { [weak self] newTelemetry in
                CarPlayManager.shared.updateCarPlayDashboard(with: newTelemetry)
            }
            .store(in: &cancellables)
    }
    
    func templateApplicationScene(_ templateApplicationScene: CPTemplateApplicationScene, didDisconnectInterfaceController interfaceController: CPInterfaceController) {
        print("[Apple CarPlay] ⚠️ Đã ngắt kết nối khỏi màn hình Apple CarPlay.")
        self.interfaceController = nil
        cancellables.removeAll()
    }
}
