package com.smartev.dashboard

import android.app.Application
import com.smartev.dashboard.ble.BleManager

class SmartEvApplication : Application() {

    lateinit var bleManager: BleManager
        private set

    override fun onCreate() {
        super.onCreate()
        instance = this
        bleManager = BleManager.getInstance(this)
    }

    companion object {
        lateinit var instance: SmartEvApplication
            private set
    }
}
