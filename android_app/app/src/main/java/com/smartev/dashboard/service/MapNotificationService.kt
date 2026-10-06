package com.smartev.dashboard.service

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import com.smartev.dashboard.ble.BleManager
import java.util.Locale

class MapNotificationService : NotificationListenerService() {

    companion object {
        private const val TAG = "SmartEV_MapService"
        private const val PKG_GOOGLE_MAPS = "com.google.android.apps.maps"
    }

    private var lastNavPrompt = ""
    private var lastMediaTitle = ""

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        super.onNotificationPosted(sbn)
        if (sbn == null) return

        val pkg = sbn.packageName
        val extras = sbn.notification.extras ?: return

        if (pkg == PKG_GOOGLE_MAPS) {
            handleGoogleMapsNotification(sbn.notification)
        } else if (isMediaNotification(sbn.notification)) {
            handleMediaNotification(extras)
        }
    }

    private fun handleGoogleMapsNotification(notification: Notification) {
        val extras = notification.extras
        val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""
        val subText = extras.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString() ?: ""

        val fullPrompt = "$title | $text | $subText"
        if (fullPrompt == lastNavPrompt || title.isEmpty()) {
            return
        }
        lastNavPrompt = fullPrompt

        Log.d(TAG, "Google Maps Nav: Title='$title', Text='$text', SubText='$subText'")

        // Phân tích hướng rẽ
        val lower = title.lowercase(Locale.ROOT) + " " + text.lowercase(Locale.ROOT)
        val icon = when {
            lower.contains("u-turn") || lower.contains("quay đầu") -> "UTURN"
            lower.contains("slight left") || lower.contains("chếch sang trái") -> "SLIGHT_LEFT"
            lower.contains("turn left") || lower.contains("rẽ trái") -> "LEFT"
            lower.contains("slight right") || lower.contains("chếch sang phải") -> "SLIGHT_RIGHT"
            lower.contains("turn right") || lower.contains("rẽ phải") -> "RIGHT"
            lower.contains("đến") || lower.contains("arrived") || lower.contains("destination") -> "DEST"
            else -> "STRAIGHT"
        }

        // Tách khoảng cách và tên đường
        var distance = "100m"
        var street = title

        // Thông thường Maps để Title là tên đường hoặc chỉ dẫn, Text là khoảng cách hoặc thời gian
        if (text.contains("m") || text.contains("km")) {
            distance = text.split("·").firstOrNull()?.trim() ?: text
        }

        BleManager.getInstance(applicationContext).sendNavigation(icon, distance, street)
    }

    private fun isMediaNotification(notification: Notification): Boolean {
        return notification.category == Notification.CATEGORY_TRANSPORT ||
                notification.extras.containsKey(Notification.EXTRA_MEDIA_SESSION)
    }

    private fun handleMediaNotification(extras: android.os.Bundle) {
        val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        val artist = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""

        if (title.isNotEmpty() && title != lastMediaTitle) {
            lastMediaTitle = title
            Log.d(TAG, "Media Playback: $title - $artist")
            BleManager.getInstance(applicationContext).sendMedia(title, artist)
        }
    }
}
