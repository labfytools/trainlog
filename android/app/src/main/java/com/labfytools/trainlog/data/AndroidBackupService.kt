package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import java.io.File
import java.io.InputStream
import java.io.OutputStream
import java.security.MessageDigest
import java.time.OffsetDateTime
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream
import org.json.JSONArray
import org.json.JSONObject

internal sealed interface AndroidBackupResult {
    data class Success(val bytes: Long) : AndroidBackupResult

    data class Error(val message: String) : AndroidBackupResult
}

internal data class VerifiedAndroidBackup(
    val directory: File,
    val database: File,
    val preferences: File,
)

/** Complete same-installation backup container; plaintext is explicit by design. */
internal class AndroidBackupService(
    private val context: Context,
    private val productVersion: String =
        checkNotNull(
            context.packageManager.getPackageInfo(context.packageName, 0).versionName,
        ),
) {
    companion object {
        const val FORMAT = "trainlog-android-backup"
        const val VERSION = 1
        const val MAX_ENTRY_BYTES = 768L * 1024L * 1024L
        const val MAX_TOTAL_BYTES = 1024L * 1024L * 1024L
        private val PREFERENCES = listOf("trainlog-sync", "trainlog_presentation_settings")
    }

    private fun digest(file: File): String {
        val value = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                value.update(buffer, 0, count)
            }
        }
        return value.digest().joinToString("") { "%02x".format(it) }
    }

    private fun preferencesJson(): String =
        JSONObject()
            .apply {
                put("format", "trainlog-android-preferences")
                put("version", 1)
                put(
                    "stores",
                    JSONObject().also { stores ->
                        PREFERENCES.forEach { name ->
                            stores.put(
                                name,
                                JSONObject().also { values ->
                                    context
                                        .getSharedPreferences(name, Context.MODE_PRIVATE)
                                        .all
                                        .toSortedMap()
                                        .forEach { (key, value) ->
                                            when (value) {
                                                null -> values.put(key, JSONObject.NULL)
                                                is String,
                                                is Boolean,
                                                is Int,
                                                is Long,
                                                is Float -> values.put(key, value)
                                                is Set<*> ->
                                                    values.put(
                                                        key,
                                                        JSONArray(
                                                            value
                                                                .filterIsInstance<String>()
                                                                .sorted()
                                                        ),
                                                    )
                                                else -> error("Unsupported preference type")
                                            }
                                        }
                                },
                            )
                        }
                    },
                )
            }
            .toString()

    fun createPrivate(repository: TrainlogRepository): Result<File> = runCatching {
        val directory = File(context.filesDir, "backups").apply { mkdirs() }
        val archive = File(directory, "trainlog-backup-${java.util.UUID.randomUUID()}.tlbackup")
        archive.outputStream().use { destination ->
            val result = writeArchive(repository, destination)
            if (result is AndroidBackupResult.Error) error(result.message)
        }
        archive
    }

    fun create(repository: TrainlogRepository, destination: OutputStream): AndroidBackupResult {
        val archive =
            createPrivate(repository).getOrElse {
                return AndroidBackupResult.Error(it.message ?: "Backup failed.")
            }
        return try {
            archive.inputStream().use { it.copyTo(destination) }
            destination.flush()
            val bytes = archive.length()
            archive.delete()
            AndroidBackupResult.Success(bytes)
        } catch (error: Exception) {
            AndroidBackupResult.Error(
                "Export failed; private backup retained at ${archive.name}: ${error.message}"
            )
        }
    }

    private fun writeArchive(
        repository: TrainlogRepository,
        destination: OutputStream,
    ): AndroidBackupResult {
        val stage = File(context.cacheDir, "backup-${java.util.UUID.randomUUID()}")
        if (!stage.mkdir())
            return AndroidBackupResult.Error("Cannot create private backup staging.")
        val database = File(stage, "database.sqlite")
        val preferences = File(stage, "preferences.json")
        return try {
            repository.backupDatabaseTo(database)
            preferences.writeText(preferencesJson())
            val schema = validateDatabase(database)
            val files = listOf("database.sqlite" to database, "preferences.json" to preferences)
            val manifest =
                JSONObject()
                    .put("format", FORMAT)
                    .put("version", VERSION)
                    .put("package", context.packageName)
                    .put("schema", schema)
                    .put("product_version", productVersion)
                    .put("plaintext", true)
                    .put("created_at", OffsetDateTime.now().toString())
                    .put(
                        "entries",
                        JSONArray(
                            files.map { (name, file) ->
                                JSONObject()
                                    .put("path", name)
                                    .put("size", file.length())
                                    .put("sha256", digest(file))
                            }
                        ),
                    )
            var written = 0L
            ZipOutputStream(destination).use { zip ->
                files.forEach { (name, file) ->
                    zip.putNextEntry(ZipEntry(name))
                    file.inputStream().use { input -> written += input.copyTo(zip) }
                    zip.closeEntry()
                }
                zip.putNextEntry(ZipEntry("manifest.json"))
                val raw = manifest.toString().toByteArray()
                zip.write(raw)
                written += raw.size
                zip.closeEntry()
            }
            AndroidBackupResult.Success(written)
        } catch (error: Exception) {
            AndroidBackupResult.Error(error.message ?: "Backup failed.")
        } finally {
            stage.deleteRecursively()
        }
    }

    fun verify(input: InputStream): Result<VerifiedAndroidBackup> {
        val stage = File(context.cacheDir, "restore-${java.util.UUID.randomUUID()}")
        if (!stage.mkdir())
            return Result.failure(IllegalStateException("Cannot create restore staging."))
        return runCatching {
                var total = 0L
                val names = mutableSetOf<String>()
                ZipInputStream(input).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        require(
                            !entry.isDirectory &&
                                entry.name in
                                    setOf("database.sqlite", "preferences.json", "manifest.json") &&
                                names.add(entry.name)
                        ) {
                            "Invalid backup entry."
                        }
                        val target = File(stage, entry.name)
                        target.outputStream().use { output ->
                            val buffer = ByteArray(64 * 1024)
                            var entryBytes = 0L
                            while (true) {
                                val count = zip.read(buffer)
                                if (count < 0) break
                                entryBytes += count
                                total += count
                                require(entryBytes <= MAX_ENTRY_BYTES && total <= MAX_TOTAL_BYTES) {
                                    "Backup exceeds bounds."
                                }
                                output.write(buffer, 0, count)
                            }
                        }
                    }
                }
                require(names == setOf("database.sqlite", "preferences.json", "manifest.json")) {
                    "Backup entries are incomplete."
                }
                val manifest = JSONObject(File(stage, "manifest.json").readText())
                require(
                    manifest.getString("format") == FORMAT &&
                        manifest.getInt("version") == VERSION &&
                        manifest.getString("package") == context.packageName &&
                        manifest.getBoolean("plaintext") &&
                        manifest.getInt("schema") in 17..25
                ) {
                    "Unsupported backup."
                }
                val expected = manifest.getJSONArray("entries")
                require(expected.length() == 2)
                val manifestPaths = mutableSetOf<String>()
                for (index in 0 until expected.length()) {
                    val item = expected.getJSONObject(index)
                    val path = item.getString("path")
                    require(
                        path in setOf("database.sqlite", "preferences.json") &&
                            manifestPaths.add(path)
                    ) {
                        "Invalid manifest entry."
                    }
                    val file = File(stage, path)
                    require(
                        file.canonicalFile.parentFile == stage.canonicalFile &&
                            file.length() == item.getLong("size") &&
                            digest(file) == item.getString("sha256")
                    ) {
                        "Backup digest mismatch."
                    }
                }
                require(manifestPaths == setOf("database.sqlite", "preferences.json")) {
                    "Backup manifest is incomplete."
                }
                require(
                    validateDatabase(File(stage, "database.sqlite")) == manifest.getInt("schema")
                )
                val preferences = JSONObject(File(stage, "preferences.json").readText())
                require(
                    preferences.getString("format") == "trainlog-android-preferences" &&
                        preferences.getInt("version") == 1
                )
                VerifiedAndroidBackup(
                    stage,
                    File(stage, "database.sqlite"),
                    File(stage, "preferences.json"),
                )
            }
            .onFailure { stage.deleteRecursively() }
    }

    fun restore(
        repository: TrainlogRepository,
        verified: VerifiedAndroidBackup,
        databaseName: String = "trainlog-android.db",
    ): AndroidBackupResult {
        val destination = context.getDatabasePath(databaseName)
        val previous =
            File(
                destination.parentFile,
                "${destination.name}.pre-restore-${java.util.UUID.randomUUID()}",
            )
        val previousPreferences =
            File(context.cacheDir, "preferences-pre-restore-${java.util.UUID.randomUUID()}.json")
        var moved = false
        return try {
            previousPreferences.writeText(preferencesJson())
            repository.close()
            destination.parentFile?.mkdirs()
            if (destination.exists()) {
                check(destination.renameTo(previous))
                moved = true
            }
            File("${destination.path}-wal").delete()
            File("${destination.path}-shm").delete()
            verified.database.copyTo(destination, overwrite = false)
            validateDatabase(destination)
            restorePreferences(verified.preferences)
            previous.delete()
            previousPreferences.delete()
            verified.directory.deleteRecursively()
            AndroidBackupResult.Success(destination.length())
        } catch (error: Exception) {
            destination.delete()
            if (moved) previous.renameTo(destination)
            if (previousPreferences.isFile) runCatching { restorePreferences(previousPreferences) }
            previousPreferences.delete()
            AndroidBackupResult.Error(error.message ?: "Restore failed.")
        }
    }

    private fun validateDatabase(file: File): Int {
        val db = SQLiteDatabase.openDatabase(file.path, null, SQLiteDatabase.OPEN_READONLY)
        try {
            require(
                db.rawQuery("PRAGMA integrity_check", null).use {
                    it.moveToFirst() && it.getString(0) == "ok"
                }
            )
            require(db.rawQuery("PRAGMA foreign_key_check", null).use { !it.moveToFirst() })
            require(db.version in 17..25) { "Unsupported Android schema ${db.version}." }
            return db.version
        } finally {
            db.close()
        }
    }

    private fun restorePreferences(file: File) {
        val stores = JSONObject(file.readText()).getJSONObject("stores")
        PREFERENCES.forEach { name ->
            val editor = context.getSharedPreferences(name, Context.MODE_PRIVATE).edit().clear()
            val values = stores.getJSONObject(name)
            values.keys().forEach { key ->
                when (val value = values.get(key)) {
                    is String -> editor.putString(key, value)
                    is Boolean -> editor.putBoolean(key, value)
                    is Int -> editor.putInt(key, value)
                    is Long -> editor.putLong(key, value)
                    is Double -> editor.putFloat(key, value.toFloat())
                    is JSONArray ->
                        editor.putStringSet(
                            key,
                            (0 until value.length()).map { value.getString(it) }.toSet(),
                        )
                }
            }
            check(editor.commit())
        }
    }
}
