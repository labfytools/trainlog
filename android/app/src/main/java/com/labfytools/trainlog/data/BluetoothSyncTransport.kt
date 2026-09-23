package com.labfytools.trainlog.data

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothClass
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.security.MessageDigest
import java.time.Duration
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream
import org.json.JSONObject

internal const val TRAINLOG_BLUETOOTH_SERVICE_UUID =
    "f0d1c0de-7a11-4f62-9b7c-545241494e4c"
internal const val TRAINLOG_BLUETOOTH_FILE_PROTOCOL = "trainlog-bt-files-v1"
private const val BLUETOOTH_FRAME_FORMAT = "trainlog-bt-frame"
private const val BLUETOOTH_FRAME_VERSION = 1
private const val MAX_BT_HEADER_BYTES = 64 * 1024
private const val MAX_BT_PAYLOAD_BYTES = 256L * 1024L * 1024L
private const val MAX_BT_FILE_BYTES = 64L * 1024L * 1024L
private const val MAX_BT_FILES = 128
private const val AUTO_REQUEST_COOLDOWN_MS = 5L * 60L * 1000L

internal val BLUETOOTH_ANDROID_PULL_COORDINATION =
    listOf(
        "android-peer-v1.json",
        FULL_GENERATION_REQUEST_NAME,
        LEGACY_SYNC_REQUEST_NAME,
        "android-generation-v1.json",
        "android-consumption-ack-v1.json",
        "android-archive-acknowledgements-v1.json",
        "android-generation-error-v1.json",
    )

internal data class BondedBluetoothDesktop(
    val name: String,
    val address: String,
)

internal class BluetoothSyncSettings(context: Context) {
    private val preferences =
        context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    val desktopAddress: String?
        get() = preferences.getString(DESKTOP_ADDRESS, null)

    val configured: Boolean
        get() = desktopAddress != null

    fun selectDesktop(address: String) {
        require(BLUETOOTH_ADDRESS.matches(address.uppercase())) {
            "Adresse Bluetooth invalide."
        }
        preferences.edit().putString(DESKTOP_ADDRESS, address.uppercase()).apply()
    }

    fun disconnect() {
        preferences.edit().remove(DESKTOP_ADDRESS).apply()
    }

    fun shouldPublishAutomaticRequest(nowMillis: Long): Boolean {
        val previous = preferences.getLong(LAST_AUTO_REQUEST, 0L)
        return previous <= 0L || nowMillis - previous >= AUTO_REQUEST_COOLDOWN_MS
    }

    fun recordAutomaticRequest(nowMillis: Long) {
        preferences.edit().putLong(LAST_AUTO_REQUEST, nowMillis).apply()
    }

    companion object {
        private const val PREFERENCES = "trainlog-bluetooth-sync"
        private const val DESKTOP_ADDRESS = "desktop_address"
        private const val LAST_AUTO_REQUEST = "last_auto_request_ms"
        private val BLUETOOTH_ADDRESS =
            Regex("^[0-9A-F]{2}(:[0-9A-F]{2}){5}$")
    }
}

internal fun hasBluetoothConnectPermission(context: Context): Boolean =
    Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
        ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) ==
        PackageManager.PERMISSION_GRANTED

@SuppressLint("MissingPermission")
internal fun bondedBluetoothDesktops(context: Context): List<BondedBluetoothDesktop> {
    if (!hasBluetoothConnectPermission(context)) return emptyList()
    val adapter =
        context.getSystemService(BluetoothManager::class.java)?.adapter
            ?: return emptyList()
    return adapter.bondedDevices
        .asSequence()
        .filter { device ->
            device.bluetoothClass?.majorDeviceClass == BluetoothClass.Device.Major.COMPUTER
        }
        .map { device ->
            BondedBluetoothDesktop(
                name = device.name?.takeIf(String::isNotBlank) ?: device.address,
                address = device.address.uppercase(),
            )
        }
        .sortedWith(compareBy<BondedBluetoothDesktop> { it.name }.thenBy { it.address })
        .toList()
}

private fun sha256(file: File): String {
    val digest = MessageDigest.getInstance("SHA-256")
    FileInputStream(file).use { input ->
        val buffer = ByteArray(64 * 1024)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
        }
    }
    return digest.digest().joinToString("") { "%02x".format(it) }
}

private data class BluetoothFrame(
    val header: JSONObject,
    val payload: File?,
) : AutoCloseable {
    override fun close() {
        payload?.delete()
    }
}

private class BluetoothFrameCodec(private val cacheDirectory: File) {
    fun read(input: DataInputStream): BluetoothFrame {
        val headerSize = input.readInt()
        if (headerSize !in 1..MAX_BT_HEADER_BYTES)
            throw IllegalStateException("En-tête Bluetooth invalide.")
        val rawHeader = ByteArray(headerSize)
        input.readFully(rawHeader)
        val header = JSONObject(rawHeader.toString(Charsets.UTF_8))
        require(
            header.optString("format") == BLUETOOTH_FRAME_FORMAT &&
                header.optInt("version", -1) == BLUETOOTH_FRAME_VERSION
        ) { "Trame Bluetooth non supportée." }
        val payloadSize =
            if (header.has("payload_size")) header.getLong("payload_size") else 0L
        require(payloadSize in 0..MAX_BT_PAYLOAD_BYTES) {
            "Charge utile Bluetooth invalide."
        }
        if (payloadSize == 0L) {
            require(!header.has("payload_sha256") || header.optString("payload_sha256").isEmpty())
            return BluetoothFrame(header, null)
        }
        val expected = header.getString("payload_sha256")
        require(expected.matches(Regex("^[0-9a-f]{64}$"))) {
            "Empreinte Bluetooth invalide."
        }
        val temporary =
            File.createTempFile("trainlog-bt-frame-", ".bin", cacheDirectory)
        try {
            val digest = MessageDigest.getInstance("SHA-256")
            FileOutputStream(temporary).use { output ->
                var remaining = payloadSize
                val buffer = ByteArray(64 * 1024)
                while (remaining > 0L) {
                    val wanted = minOf(buffer.size.toLong(), remaining).toInt()
                    val count = input.read(buffer, 0, wanted)
                    if (count < 0) throw IllegalStateException("Connexion Bluetooth interrompue.")
                    output.write(buffer, 0, count)
                    digest.update(buffer, 0, count)
                    remaining -= count.toLong()
                }
                output.fd.sync()
            }
            val actual = digest.digest().joinToString("") { "%02x".format(it) }
            require(actual == expected) { "Empreinte Bluetooth incorrecte." }
            return BluetoothFrame(header, temporary)
        } catch (error: Exception) {
            temporary.delete()
            throw error
        }
    }

    fun write(output: DataOutputStream, header: JSONObject, payload: File? = null) {
        val payloadSize = payload?.length() ?: 0L
        require(payloadSize in 0..MAX_BT_PAYLOAD_BYTES)
        val value =
            JSONObject(header.toString())
                .put("format", BLUETOOTH_FRAME_FORMAT)
                .put("version", BLUETOOTH_FRAME_VERSION)
                .put("payload_size", payloadSize)
        if (payload != null) {
            require(payload.isFile)
            value.put("payload_sha256", sha256(payload))
        }
        val raw = value.toString().toByteArray(Charsets.UTF_8)
        require(raw.size in 1..MAX_BT_HEADER_BYTES)
        output.writeInt(raw.size)
        output.write(raw)
        if (payload != null) {
            FileInputStream(payload).use { input -> input.copyTo(output, 64 * 1024) }
        }
        output.flush()
    }
}

private class BluetoothExchangeArchive(
    private val context: Context,
) {
    private val exchangeDirectory = canonicalExchangeDirectory()

    private fun safeRelative(relative: String): List<String> {
        require(relative.isNotBlank() && '\\' !in relative)
        val parts = relative.split('/')
        require(parts.size in 1..6 && parts.all { it.isNotBlank() && it != "." && it != ".." })
        return parts
    }

    private fun addFile(
        archive: ZipOutputStream,
        file: File,
        relative: String,
        seen: MutableSet<String>,
        totals: LongArray,
    ) {
        if (!file.isFile || Files.isSymbolicLink(file.toPath())) return
        safeRelative(relative)
        require(seen.add(relative)) { "Chemin Bluetooth dupliqué." }
        val size = file.length()
        require(size in 0..MAX_BT_FILE_BYTES) { "Fichier Bluetooth trop volumineux." }
        totals[0] += 1
        totals[1] = Math.addExact(totals[1], size)
        require(totals[0] <= MAX_BT_FILES && totals[1] <= MAX_BT_PAYLOAD_BYTES) {
            "Archive Bluetooth trop volumineuse."
        }
        archive.putNextEntry(ZipEntry(relative).also { it.size = size })
        FileInputStream(file).use { input -> input.copyTo(archive, 64 * 1024) }
        archive.closeEntry()
    }

    fun buildAndroidPullArchive(): File {
        require(exchangeDirectory.isDirectory) { "Dossier d'échange Trainlog indisponible." }
        val output = File.createTempFile("trainlog-bt-pull-", ".zip", context.cacheDir)
        val seen = linkedSetOf<String>()
        val totals = longArrayOf(0L, 0L)
        try {
            ZipOutputStream(BufferedOutputStream(FileOutputStream(output))).use { zip ->
                BLUETOOTH_ANDROID_PULL_COORDINATION.forEach { name ->
                    addFile(zip, File(exchangeDirectory, name), name, seen, totals)
                }
                val reference = File(exchangeDirectory, "android-generation-v1.json")
                if (reference.isFile && reference.length() <= MAX_BT_HEADER_BYTES) {
                    val value = JSONObject(reference.readText())
                    val relative = value.optString("relative_path")
                    val generationId = value.optString("generation_id")
                    val expected = "android-objects/generations/$generationId"
                    require(relative == expected) { "Référence de génération Android invalide." }
                    val directory = File(exchangeDirectory, relative)
                    require(directory.isDirectory && !Files.isSymbolicLink(directory.toPath()))
                    directory.listFiles().orEmpty().sortedBy(File::getName).forEach { file ->
                        addFile(zip, file, "$relative/${file.name}", seen, totals)
                    }
                }
            }
            require(output.length() <= MAX_BT_PAYLOAD_BYTES)
            return output
        } catch (error: Exception) {
            output.delete()
            throw error
        }
    }

    fun applyDesktopPushArchive(archiveFile: File) {
        require(archiveFile.isFile && archiveFile.length() in 1..MAX_BT_PAYLOAD_BYTES)
        if (!exchangeDirectory.exists()) require(exchangeDirectory.mkdirs())
        require(exchangeDirectory.isDirectory)
        val root = exchangeDirectory.canonicalFile
        val seen = linkedSetOf<String>()
        val published = mutableListOf<File>()
        var count = 0L
        var total = 0L
        ZipFile(archiveFile).use { archive ->
            val entries = archive.entries()
            while (entries.hasMoreElements()) {
                val entry = entries.nextElement()
                if (entry.isDirectory) continue
                val parts = safeRelative(entry.name)
                require(seen.add(entry.name)) { "Chemin Bluetooth dupliqué." }
                require(entry.size in 0..MAX_BT_FILE_BYTES) { "Fichier Bluetooth trop volumineux." }
                count += 1
                total = Math.addExact(total, entry.size)
                require(count <= MAX_BT_FILES && total <= MAX_BT_PAYLOAD_BYTES)
                val destination =
                    parts.fold(root) { parent, name -> File(parent, name) }
                val parent = requireNotNull(destination.parentFile)
                require(parent.mkdirs() || parent.isDirectory)
                require(parent.canonicalFile.toPath().startsWith(root.toPath())) {
                    "Chemin Bluetooth hors du dossier Trainlog."
                }
                val temporary =
                    File(parent, ".${destination.name}.bt-${UUID.randomUUID()}.tmp")
                try {
                    archive.getInputStream(entry).use { input ->
                        FileOutputStream(temporary).use { output ->
                            var copied = 0L
                            val buffer = ByteArray(64 * 1024)
                            while (true) {
                                val read = input.read(buffer)
                                if (read < 0) break
                                copied += read.toLong()
                                require(copied <= entry.size)
                                output.write(buffer, 0, read)
                            }
                            require(copied == entry.size)
                            output.fd.sync()
                        }
                    }
                    try {
                        Files.move(
                            temporary.toPath(),
                            destination.toPath(),
                            StandardCopyOption.ATOMIC_MOVE,
                            StandardCopyOption.REPLACE_EXISTING,
                        )
                    } catch (_: AtomicMoveNotSupportedException) {
                        Files.move(
                            temporary.toPath(),
                            destination.toPath(),
                            StandardCopyOption.REPLACE_EXISTING,
                        )
                    }
                    published += destination
                } catch (error: Exception) {
                    temporary.delete()
                    throw error
                }
            }
        }
        AndroidMtpPublicationVisibility(context).confirm(published)
        if (AI_EXPORT_FILENAME in seen) {
            AiExportDriveSettings.enqueueImmediate(context)
        }
    }
}

internal class BluetoothSyncClient(
    private val context: Context,
    private val repository: TrainlogRepository,
) {
    companion object {
        private const val TAG = "TrainlogBluetoothSync"
    }
    @Volatile private var activeSocket: BluetoothSocket? = null
    private val settings = BluetoothSyncSettings(context)
    private val codec = BluetoothFrameCodec(context.cacheDir)
    private val archive = BluetoothExchangeArchive(context)

    @SuppressLint("MissingPermission")
    fun run(stopped: AtomicBoolean) {
        while (!stopped.get()) {
            if (!settings.configured || !hasBluetoothConnectPermission(context)) {
                sleep(stopped, Duration.ofSeconds(10))
                continue
            }
            val adapter =
                context.getSystemService(BluetoothManager::class.java)?.adapter
            if (adapter == null || !adapter.isEnabled) {
                sleep(stopped, Duration.ofSeconds(10))
                continue
            }
            val address = settings.desktopAddress ?: continue
            val device =
                runCatching { adapter.getRemoteDevice(address) }
                    .onFailure { Log.w(TAG, "Configured Bluetooth desktop is invalid", it) }
                    .getOrNull()
            if (device == null || !ensureBond(device, stopped)) {
                sleep(stopped, Duration.ofSeconds(5))
                continue
            }
            val socket =
                runCatching {
                    device.createRfcommSocketToServiceRecord(
                        UUID.fromString(TRAINLOG_BLUETOOTH_SERVICE_UUID)
                    )
                }
                    .onFailure { Log.w(TAG, "Unable to create RFCOMM socket", it) }
                    .getOrNull()
            if (socket == null) {
                sleep(stopped, Duration.ofSeconds(10))
                continue
            }
            activeSocket = socket
            try {
                socket.connect()
                Log.i(TAG, "RFCOMM connected to selected Trainlog desktop")
                serve(socket, stopped)
            } catch (error: Exception) {
                Log.w(TAG, "RFCOMM connection ended: ${error.message}", error)
            } finally {
                activeSocket = null
                runCatching { socket.close() }
            }
            if (!stopped.get()) sleep(stopped, Duration.ofSeconds(5))
        }
    }

    fun shutdown() {
        runCatching { activeSocket?.close() }
        activeSocket = null
    }

    @SuppressLint("MissingPermission")
    private fun ensureBond(device: BluetoothDevice, stopped: AtomicBoolean): Boolean {
        if (device.bondState == BluetoothDevice.BOND_BONDED) return true
        if (device.bondState == BluetoothDevice.BOND_NONE) {
            Log.i(TAG, "Bluetooth bond missing; requesting one-time repair")
            if (!runCatching { device.createBond() }.getOrDefault(false)) {
                Log.w(TAG, "Bluetooth bond request was rejected by Android")
                return false
            }
        }
        val deadline = System.nanoTime() + Duration.ofSeconds(30).toNanos()
        while (!stopped.get() && System.nanoTime() < deadline) {
            when (device.bondState) {
                BluetoothDevice.BOND_BONDED -> {
                    Log.i(TAG, "Bluetooth bond ready")
                    return true
                }
                BluetoothDevice.BOND_NONE -> {
                    Log.w(TAG, "Bluetooth bond repair failed")
                    return false
                }
            }
            sleep(stopped, Duration.ofMillis(250))
        }
        return device.bondState == BluetoothDevice.BOND_BONDED
    }

    private fun sleep(stopped: AtomicBoolean, duration: Duration) {
        val deadline = System.nanoTime() + duration.toNanos()
        while (!stopped.get() && System.nanoTime() < deadline) {
            try {
                Thread.sleep(250)
            } catch (_: InterruptedException) {
                Thread.currentThread().interrupt()
                return
            }
        }
    }

    private fun serve(socket: BluetoothSocket, stopped: AtomicBoolean) {
        val input = DataInputStream(BufferedInputStream(socket.inputStream))
        val output = DataOutputStream(BufferedOutputStream(socket.outputStream))
        val peer = SyncGenerationService(repository).peerId()
        codec.write(
            output,
            JSONObject()
                .put("type", "hello")
                .put("protocol", TRAINLOG_BLUETOOTH_FILE_PROTOCOL)
                .put("peer_id", peer),
        )
        codec.read(input).use { response ->
            require(
                response.payload == null &&
                    response.header.optString("type") == "hello_ack" &&
                    response.header.optString("protocol") == TRAINLOG_BLUETOOTH_FILE_PROTOCOL &&
                    response.header.optString("peer_id") == peer
            ) { "Accusé Bluetooth Trainlog invalide." }
        }

        publishArrivalRequest()

        while (!stopped.get()) {
            codec.read(input).use { frame ->
                val type = frame.header.optString("type")
                val requestId = frame.header.optString("request_id")
                require(requestId.startsWith("bt_")) { "Requête Bluetooth non corrélée." }
                when (type) {
                    "pull" -> {
                        require(frame.payload == null)
                        val payload = archive.buildAndroidPullArchive()
                        try {
                            codec.write(
                                output,
                                JSONObject()
                                    .put("type", "result")
                                    .put("request_id", requestId)
                                    .put("result", "ok"),
                                payload,
                            )
                        } finally {
                            payload.delete()
                        }
                    }
                    "push" -> {
                        val payload = requireNotNull(frame.payload) {
                            "Archive Bluetooth desktop absente."
                        }
                        archive.applyDesktopPushArchive(payload)
                        codec.write(
                            output,
                            JSONObject()
                                .put("type", "result")
                                .put("request_id", requestId)
                                .put("result", "ok"),
                        )
                    }
                    else -> throw IllegalStateException("Opération Bluetooth non supportée.")
                }
            }
        }
    }

    private fun publishArrivalRequest() {
        if (!BackgroundSyncSettings(context).enabled) return
        val now = System.currentTimeMillis()
        if (!settings.shouldPublishAutomaticRequest(now)) return
        val directory = canonicalExchangeDirectory()
        if (!directory.isDirectory) return
        val coordinator =
            SyncGenerationCoordinator(repository, AndroidMtpPublicationVisibility(context))
        coordinator.publishPeer(directory)
        when (SyncRequestOutbox(context).requestFullGeneration()) {
            is SyncRequestResult.Requested,
            is SyncRequestResult.Completed -> settings.recordAutomaticRequest(now)
            else -> Unit
        }
    }
}
