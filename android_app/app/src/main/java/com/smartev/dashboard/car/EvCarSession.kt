package com.smartev.dashboard.car

import android.content.Intent
import androidx.car.app.Screen
import androidx.car.app.Session

/**
 * Quản lý phiên làm việc của Smart EV Dashboard trên Android Auto
 */
class EvCarSession : Session() {

    override fun onCreateScreen(intent: Intent): Screen {
        return EvDashboardScreen(carContext)
    }
}
