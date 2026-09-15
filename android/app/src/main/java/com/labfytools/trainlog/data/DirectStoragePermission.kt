/*
 * Android DirectStoragePermission.
 *
 * Owns this data-layer boundary while keeping UI state, canonical desktop history, and exchange contracts separate.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.Settings

/* CONTRACT: only Android's settings UI can grant all-files access. Prefer the
 * package-scoped screen and fall back to the platform-wide screen when an OEM
 * does not expose the former activity. */
internal fun directStoragePermissionIntent(context: Context): Intent {
    val packageIntent = Intent(
        Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
        Uri.parse("package:${context.packageName}"),
    )
    return if (packageIntent.resolveActivity(context.packageManager) != null) {
        packageIntent
    } else {
        Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
    }
}
