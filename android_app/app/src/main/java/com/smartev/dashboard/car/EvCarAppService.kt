package com.smartev.dashboard.car

import android.content.Intent
import androidx.car.app.CarAppService
import androidx.car.app.Session
import androidx.car.app.validation.HostValidator

/**
 * Service chính chịu trách nhiệm giao tiếp với hệ điều hành Android Auto trên xe
 */
class EvCarAppService : CarAppService() {

    override fun createHostValidator(): HostValidator {
        // Cho phép kết nối trên tất cả Head Unit (Android Auto chính thức & Desktop Head Unit thử nghiệm)
        return HostValidator.ALLOW_ALL_HOSTS_VALIDATOR
    }

    override fun onCreateSession(): Session {
        return EvCarSession()
    }
}
