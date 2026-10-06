import SwiftUI

@main
struct SmartEVDashboardApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    var body: some Scene {
        WindowGroup {
            iPhoneDashboardView(bleManager: appDelegate.bleManager)
        }
    }
}

/// AppDelegate managing shared state and SceneDelegate configurations
class AppDelegate: NSObject, UIApplicationDelegate {
    let bleManager = ESP32BLEManager()
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        print("[Smart EV Dashboard] iOS App started with Apple CarPlay support!")
        return true
    }
    
    // MARK: - CarPlay & iPhone Scene Configurations
    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        
        if connectingSceneSession.role == UISceneSession.Role.carTemplateApplication {
            let sceneConfig = UISceneConfiguration(name: "CarPlay", sessionRole: connectingSceneSession.role)
            sceneConfig.delegateClass = CarPlaySceneDelegate.self
            return sceneConfig
        } else {
            let sceneConfig = UISceneConfiguration(name: "Phone", sessionRole: connectingSceneSession.role)
            return sceneConfig
        }
    }
}
