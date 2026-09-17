package com.labfytools.trainlog.data

import java.io.File
import java.nio.ByteBuffer
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.security.MessageDigest
import java.time.OffsetDateTime
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject

internal class SyncGenerationException(message: String) : IllegalArgumentException(message)

internal data class CapturedSyncGeneration(
    val generationId: String,
    val runId: String,
    val manifestSha256: String,
    val stagingDirectory: File,
)

/**
 * Staged generation service. It is deliberately not wired into the active V3 transport. WHY:
 * manifest publication and peer consumption need durable, independently retryable identities.
 * CONTRACT: artifacts are immutable, bounded and published before the manifest marker. INVARIANT:
 * causal operation payloads are never rewritten to attach generation identity.
 */
internal class SyncGenerationService(private val repository: TrainlogRepository) {
    companion object {
        const val MAX_MANIFEST_BYTES = 64 * 1024
        const val MAX_ARTIFACTS = 32
        const val MAX_ARTIFACT_BYTES = 64L * 1024L * 1024L
        const val MAX_GENERATION_BYTES = 256L * 1024L * 1024L
        const val MAX_LOGICAL_NAME_BYTES = 64
        const val MAX_PATH_BYTES = 240
        const val MAX_DIAGNOSTIC_BYTES = 1024
        const val MAX_RETAINED_PER_PEER = 8
        private val ID =
            Regex(
                "^(gen|peer|sy)_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
            )
        private val DIGEST = Regex("^[0-9a-f]{64}$")

        private data class Kind(
            val format: String,
            val version: Int,
            val filename: String,
            val required: Boolean = true,
        )

        private val ANDROID_KINDS =
            linkedMapOf(
                "catalog" to Kind("trainlog-pc-catalog", 1, "catalog-v1.json"),
                "history" to Kind("trainlog-mobile-export", 4, "history-v4.json"),
                "execution-drafts" to
                    Kind("trainlog-execution-drafts", 1, "execution-drafts-v1.json"),
                "exercise-aliases" to
                    Kind("trainlog-exercise-aliases", 1, "exercise-aliases-v1.json"),
                "exercise-profile-state" to
                    Kind("trainlog-exercise-profile-state", 1, "exercise-profile-state-v1.json"),
                "equipment-definitions" to
                    Kind("trainlog-equipment-definitions", 1, "equipment-definitions-v1.json"),
                "equipment-associations" to
                    Kind("trainlog-equipment-associations", 2, "equipment-associations-v2.json"),
                "body-zones" to Kind("trainlog-exercise-body-zones", 1, "body-zones-v1.json"),
                "feedback" to Kind("trainlog-training-feedback", 2, "feedback-v2.json"),
                "causal-deletions" to
                    Kind("trainlog-causal-deletions", 1, "causal-deletions-v1.json"),
            )
        private val SUPPORTED =
            (ANDROID_KINDS.values +
                    Kind("trainlog-ai-session-drafts", 1, "ai-proposals-v1.json", false) +
                    Kind("trainlog-session-preparations", 1, "session-preparations-v1.json", false))
                .map { it.format to it.version }
                .toSet()

        private fun sha256(bytes: ByteArray): String =
            MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") {
                "%02x".format(it)
            }

        private fun canonical(value: Any?): String =
            when (value) {
                null,
                JSONObject.NULL -> "null"
                is JSONObject ->
                    value.keys().asSequence().toList().sorted().joinToString(",", "{", "}") {
                        JSONObject.quote(it) + ":" + canonical(value.get(it))
                    }
                is JSONArray ->
                    (0 until value.length()).joinToString(",", "[", "]") {
                        canonical(value.get(it))
                    }
                is String -> JSONObject.quote(value)
                is Boolean,
                is Int,
                is Long -> value.toString()
                else -> throw SyncGenerationException("unsupported manifest JSON value")
            }

        private fun requireId(value: String, prefix: String) {
            if (!value.startsWith("${prefix}_") || !ID.matches(value))
                throw SyncGenerationException("invalid $prefix identity")
        }

        private fun safePath(value: String, generationId: String) {
            if (
                value.toByteArray(StandardCharsets.UTF_8).size > MAX_PATH_BYTES ||
                    '\u0000' in value ||
                    '\\' in value ||
                    value.startsWith('/')
            )
                throw SyncGenerationException("unsafe artifact path")
            val parts = value.split('/')
            if (
                parts.size != 3 ||
                    parts.any { it.isEmpty() || it == "." || it == ".." } ||
                    parts[0] != "generations" ||
                    parts[1] != generationId
            )
                throw SyncGenerationException("artifact outside generation namespace")
        }

        private fun boundedJson(root: Any) {
            val stack = java.util.ArrayDeque<Pair<Any, Int>>()
            stack.add(root to 1)
            var nodes = 0
            while (stack.isNotEmpty()) {
                val (value, depth) = stack.removeLast()
                nodes++
                if (depth > 8 || nodes > 8192)
                    throw SyncGenerationException("JSON nesting/node bound exceeded")
                when (value) {
                    is JSONObject -> value.keys().forEach { stack.add(value.get(it) to depth + 1) }
                    is JSONArray ->
                        for (index in 0 until value.length()) stack.add(
                            value.get(index) to depth + 1
                        )
                }
            }
        }
    }

    private fun stablePeerId(kind: String): String =
        repository.inSyncGenerationTransaction { db ->
            db.rawQuery("SELECT peer_id,kind FROM sync_peer_identity WHERE singleton=1", null)
                .use { cursor ->
                    if (cursor.moveToFirst()) {
                        if (cursor.getString(1) != kind)
                            throw SyncGenerationException("peer kind conflict")
                        return@inSyncGenerationTransaction cursor.getString(0)
                    }
                }
            val value = "peer_${UUID.randomUUID()}"
            db.execSQL(
                "INSERT INTO sync_peer_identity(singleton,peer_id,kind) VALUES(1,?,?)",
                arrayOf(value, kind),
            )
            value
        }

    /* CONTRACT: capability advertisements bind to this installation identity;
     * callers cannot supply or replace it. */
    internal fun peerId(): String = stablePeerId("android")

    private fun archiveDigest(directory: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        directory.walkTopDown().filter { it.isFile }.sortedBy { it.relativeTo(directory).path }
            .forEach { file ->
                if (java.nio.file.Files.isSymbolicLink(file.toPath()))
                    throw SyncGenerationException("archive contains symbolic link")
                val name = file.relativeTo(directory).invariantSeparatorsPath.toByteArray()
                digest.update(ByteBuffer.allocate(4).putInt(name.size).array())
                digest.update(name)
                digest.update(MessageDigest.getInstance("SHA-256").digest(file.readBytes()))
            }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    internal fun archiveAcknowledged(ownedRoot: File, consumerPeerId: String): Int {
        /* WHY: acknowledged payloads must not occupy active admission forever.
         * CONTRACT: an exact consumed ACK is required and the newest two
         * acknowledged lineage members remain active for recovery.
         * INVARIANT: generation, artifact, ACK and causal rows remain intact;
         * the ledger is committed only after a verified atomic archive copy. */
        val rows = repository.inSyncGenerationTransaction { db ->
            db.rawQuery(
                "SELECT g.generation_id,g.run_id,g.producer_peer_id,g.consumer_peer_id," +
                    "g.manifest_sha256,g.staging_path,g.manifest_json FROM sync_generations g " +
                    "WHERE g.consumer_peer_id=? AND g.status='acknowledged' AND NOT EXISTS(" +
                    "SELECT 1 FROM sync_generation_archives a WHERE a.generation_id=g.generation_id) " +
                    "ORDER BY g.generated_at DESC,g.generation_id DESC",
                arrayOf(consumerPeerId),
            ).use { cursor ->
                buildList {
                    while (cursor.moveToNext()) add(List(7) { cursor.getString(it) })
                }
            }
        }
        val root = File(File(ownedRoot, "archives"), "generations")
        if (!root.exists() && !root.mkdirs())
            throw SyncGenerationException("cannot create generation archive")
        var count = 0
        rows.drop(2).forEach { row ->
            val eligible = repository.inSyncGenerationTransaction { db ->
                db.rawQuery(
                    "SELECT ack_id FROM sync_acknowledgements WHERE generation_id=? AND run_id=? " +
                        "AND producer_peer_id=? AND consumer_peer_id=? AND manifest_sha256=? " +
                        "AND result='consumed' AND durability='sqlite-commit-full' LIMIT 2",
                    arrayOf(row[0], row[1], row[2], row[3], row[4]),
                ).use { cursor ->
                    if (!cursor.moveToFirst()) null
                    else cursor.getString(0).also {
                        if (cursor.moveToNext()) throw SyncGenerationException("ambiguous ACK evidence")
                    }
                }
            } ?: return@forEach
            val source = File(row[5])
            if (Files.isSymbolicLink(source.toPath()))
                throw SyncGenerationException("generation staging is a symbolic link")
            validatePublished(source, row[3])
            val destination = File(root, row[0])
            val temporary = File(root, ".${row[0]}.tmp")
            if (Files.isSymbolicLink(destination.toPath()))
                throw SyncGenerationException("generation archive is a symbolic link")
            if (!destination.exists()) {
                if (Files.isSymbolicLink(temporary.toPath()))
                    throw SyncGenerationException("temporary generation archive is a symbolic link")
                if (temporary.exists() && !temporary.deleteRecursively())
                    throw SyncGenerationException("cannot reset interrupted generation archive")
                if (!source.copyRecursively(temporary, overwrite = false))
                    throw SyncGenerationException("cannot copy generation archive")
                validatePublished(temporary, row[3])
                temporary.walkTopDown().filter { it.isFile }.forEach { file ->
                    java.io.FileOutputStream(file, true).use { it.fd.sync() }
                }
                if (!temporary.renameTo(destination))
                    throw SyncGenerationException("cannot commit generation archive")
                /* Android emulated external storage rejects directory fsync.
                 * Reopen and sync the commit marker after the atomic rename so
                 * the supported FUSE durability barrier covers the committed
                 * namespace before the internal SQLite ledger can advance. */
                java.io.FileOutputStream(File(destination, "manifest.json"), true).use {
                    it.fd.sync()
                }
            }
            validatePublished(destination, row[3])
            val checksum = archiveDigest(destination)
            val audit =
                canonical(
                    JSONObject()
                        .put("format", "trainlog-sync-generation-archive-audit")
                        .put("version", 1)
                        .put("generation_id", row[0])
                        .put("run_id", row[1])
                        .put("manifest_sha256", row[4])
                        .put("ack_id", eligible)
                        .put("archive_sha256", checksum),
                )
            repository.inSyncGenerationTransaction { db ->
                db.execSQL(
                    "INSERT OR IGNORE INTO sync_generation_archives VALUES(?,?,?,?,?,?)",
                    arrayOf(row[0], destination.absolutePath, row[4], checksum, OffsetDateTime.now().toString(), audit),
                )
            }
            count += 1
        }
        return count
    }

    fun capture(
        ownedRoot: File,
        consumerPeerId: String,
        runId: String = "sy_${UUID.randomUUID()}",
        generationId: String = "gen_${UUID.randomUUID()}",
    ): CapturedSyncGeneration {
        requireId(consumerPeerId, "peer")
        requireId(runId, "sy")
        requireId(generationId, "gen")
        archiveAcknowledged(ownedRoot, consumerPeerId)
        repository.inSyncGenerationTransaction { db ->
            val retained =
                db.rawQuery(
                    "SELECT COUNT(*) FROM sync_generations g WHERE consumer_peer_id=? AND status IN('captured','published','waiting_acknowledgement','acknowledged') AND NOT EXISTS(SELECT 1 FROM sync_generation_archives a WHERE a.generation_id=g.generation_id)",
                        arrayOf(consumerPeerId),
                    )
                    .use {
                        it.moveToFirst()
                        it.getInt(0)
                    }
            if (retained >= MAX_RETAINED_PER_PEER)
                throw SyncGenerationException("generation retention capacity exhausted")
        }
        val producer = stablePeerId("android")
        val stage = File(File(ownedRoot, "staging"), generationId)
        if (stage.exists() || !stage.mkdirs())
            throw SyncGenerationException("generation staging already exists")
        try {
            val artifacts = repository.captureSyncGenerationArtifacts()
            if (artifacts.keys != ANDROID_KINDS.keys)
                throw SyncGenerationException("incomplete Android domain capture")
            val descriptors = JSONArray()
            var total = 0L
            artifacts.forEach { (logical, json) ->
                val kind = checkNotNull(ANDROID_KINDS[logical])
                val bytes = json.toByteArray(StandardCharsets.UTF_8)
                if (bytes.size.toLong() > MAX_ARTIFACT_BYTES)
                    throw SyncGenerationException("artifact exceeds 64 MiB")
                total = Math.addExact(total, bytes.size.toLong())
                if (total > MAX_GENERATION_BYTES)
                    throw SyncGenerationException("generation exceeds 256 MiB")
                val output = File(stage, kind.filename)
                output.outputStream().use { it.write(bytes) }
                output.setReadable(false, false)
                output.setReadable(true, true)
                output.setWritable(false, false)
                output.setWritable(true, true)
                descriptors.put(
                    JSONObject()
                        .put("logical_name", logical)
                        .put("format", kind.format)
                        .put("version", kind.version)
                        .put("filename", "generations/$generationId/${kind.filename}")
                        .put("size", bytes.size)
                        .put("sha256", sha256(bytes))
                        .put("required", kind.required)
                )
            }
            val generatedAt = OffsetDateTime.now().toString()
            val parent =
                repository.inSyncGenerationTransaction { db ->
                    db.rawQuery(
                            "SELECT g.generation_id FROM sync_generations g WHERE g.producer_peer_id=? AND g.consumer_peer_id=? AND g.status='acknowledged' AND NOT EXISTS(SELECT 1 FROM sync_generations c WHERE c.parent_generation_id=g.generation_id AND c.status='acknowledged') LIMIT 2",
                            arrayOf(producer, consumerPeerId),
                        )
                        .use {
                            if (!it.moveToFirst()) null
                            else {
                                val value = it.getString(0)
                                if (it.moveToNext())
                                    throw SyncGenerationException(
                                        "outgoing generation lineage has multiple tips"
                                    )
                                value
                            }
                        }
                }
            val manifest =
                JSONObject()
                    .put("format", "trainlog-sync-manifest")
                    .put("version", 1)
                    .put("generation_id", generationId)
                    .put("run_id", runId)
                    .put("producer", JSONObject().put("peer_id", producer).put("kind", "android"))
                    .put("consumer_peer_id", consumerPeerId)
                    .put("generated_at", generatedAt)
                    .put("parent_generation_id", parent ?: JSONObject.NULL)
                    .put("artifacts", descriptors)
            val raw = canonical(manifest).toByteArray(StandardCharsets.UTF_8)
            validateManifest(raw, consumerPeerId)
            val digest = sha256(raw)
            File(stage, "manifest.json").outputStream().use { it.write(raw) }
            repository.inSyncGenerationTransaction { db ->
                db.execSQL(
                    "INSERT INTO sync_generations VALUES(?,?,?,?,?,?,?,?,?,?,?,NULL)",
                    arrayOf(
                        generationId,
                        runId,
                        producer,
                        consumerPeerId,
                        "android",
                        generatedAt,
                        parent,
                        digest,
                        "captured",
                        raw.toString(StandardCharsets.UTF_8),
                        stage.absolutePath,
                    ),
                )
                for (index in 0 until descriptors.length()) {
                    val item = descriptors.getJSONObject(index)
                    db.execSQL(
                        "INSERT INTO sync_generation_artifacts VALUES(?,?,?,?,?,?,?,?)",
                        arrayOf(
                            generationId,
                            item.getString("logical_name"),
                            item.getString("format"),
                            item.getInt("version"),
                            item.getString("filename"),
                            item.getLong("size"),
                            item.getString("sha256"),
                            if (item.getBoolean("required")) 1 else 0,
                        ),
                    )
                }
                val causal = JSONObject(checkNotNull(artifacts["causal-deletions"]))
                val operations = causal.getJSONArray("operations")
                for (index in 0 until operations.length()) {
                    val operationId = operations.getJSONObject(index).getString("operation_id")
                    val existed =
                        db.rawQuery(
                                "SELECT 1 FROM sync_causal_publications WHERE operation_id=? LIMIT 1",
                                arrayOf(operationId),
                            )
                            .use { it.moveToFirst() }
                    db.execSQL(
                        "INSERT INTO sync_causal_publications VALUES(?,?,?)",
                        arrayOf(operationId, generationId, if (existed) 0 else 1),
                    )
                }
            }
            return CapturedSyncGeneration(generationId, runId, digest, stage)
        } catch (error: Exception) {
            stage.deleteRecursively()
            throw error
        }
    }

    fun publish(captured: CapturedSyncGeneration, objectRoot: File): File {
        val destination = File(File(objectRoot, "generations"), captured.generationId)
        val sourceManifest = File(captured.stagingDirectory, "manifest.json").readBytes()
        val manifest = validateManifest(sourceManifest, null)
        val marker = File(destination, "manifest.json")
        if (marker.isFile) {
            if (sha256(marker.readBytes()) != captured.manifestSha256)
                throw SyncGenerationException("same generation has different published bytes")
            validatePublished(destination, manifest.getString("consumer_peer_id"))
        } else {
            if (
                destination.exists() &&
                    (!destination.isDirectory ||
                        java.nio.file.Files.isSymbolicLink(destination.toPath()))
            )
                throw SyncGenerationException("unsafe generation namespace")
            if (!destination.exists() && !destination.mkdirs())
                throw SyncGenerationException("cannot create generation namespace")
            val values = manifest.getJSONArray("artifacts")
            for (index in 0 until values.length()) {
                val name = values.getJSONObject(index).getString("filename").substringAfterLast('/')
                val item = values.getJSONObject(index)
                val source = File(captured.stagingDirectory, name)
                if (
                    !source.isFile ||
                        java.nio.file.Files.isSymbolicLink(source.toPath()) ||
                        source.canonicalFile.parentFile !=
                            captured.stagingDirectory.canonicalFile ||
                        source.length() != item.getLong("size") ||
                        sha256(source.readBytes()) != item.getString("sha256")
                )
                    throw SyncGenerationException("staging artifact changed after capture")
                val target = File(destination, name)
                if (target.exists()) {
                    if (
                        java.nio.file.Files.isSymbolicLink(target.toPath()) ||
                            target.length() != item.getLong("size") ||
                            sha256(target.readBytes()) != item.getString("sha256")
                    )
                        throw SyncGenerationException("conflicting partial publication")
                } else
                    source.inputStream().use { input ->
                        target.outputStream().use { output -> input.copyTo(output, 1024 * 1024) }
                    }
            }
            // The manifest is the visibility/commit marker and is always last.
            marker.outputStream().use { it.write(sourceManifest) }
        }
        repository.inSyncGenerationTransaction { db ->
            db.execSQL(
                "UPDATE sync_generations SET status='waiting_acknowledgement' WHERE generation_id=? AND status IN('captured','published','waiting_acknowledgement')",
                arrayOf(captured.generationId),
            )
        }
        return destination
    }

    fun validateManifest(raw: ByteArray, expectedConsumer: String?): JSONObject {
        if (
            raw.size > MAX_MANIFEST_BYTES ||
                !repository.syncGenerationJsonHasUniqueKeys(raw.toString(StandardCharsets.UTF_8))
        )
            throw SyncGenerationException("invalid or oversized manifest JSON")
        val root =
            try {
                JSONObject(raw.toString(StandardCharsets.UTF_8))
            } catch (_: Exception) {
                throw SyncGenerationException("invalid manifest JSON")
            }
        boundedJson(root)
        val keys =
            setOf(
                "format",
                "version",
                "generation_id",
                "run_id",
                "producer",
                "consumer_peer_id",
                "generated_at",
                "parent_generation_id",
                "artifacts",
            )
        if (
            root.keys().asSequence().toSet() != keys ||
                root.optString("format") != "trainlog-sync-manifest" ||
                root.opt("version") !is Int ||
                root.getInt("version") != 1
        )
            throw SyncGenerationException("unsupported manifest shape/version")
        val generation = root.getString("generation_id")
        requireId(generation, "gen")
        requireId(root.getString("run_id"), "sy")
        val producer = root.getJSONObject("producer")
        if (
            producer.keys().asSequence().toSet() != setOf("peer_id", "kind") ||
                producer.getString("kind") !in setOf("android", "desktop")
        )
            throw SyncGenerationException("invalid producer")
        requireId(producer.getString("peer_id"), "peer")
        val consumer = root.getString("consumer_peer_id")
        requireId(consumer, "peer")
        if (expectedConsumer != null && consumer != expectedConsumer)
            throw SyncGenerationException("wrong consumer")
        if (consumer == producer.getString("peer_id"))
            throw SyncGenerationException("producer and consumer must differ")
        if (!root.isNull("parent_generation_id")) {
            requireId(root.getString("parent_generation_id"), "gen")
            if (root.getString("parent_generation_id") == generation)
                throw SyncGenerationException("generation cannot parent itself")
        }
        try {
            OffsetDateTime.parse(root.getString("generated_at"))
        } catch (_: Exception) {
            throw SyncGenerationException("invalid generated_at")
        }
        val values = root.getJSONArray("artifacts")
        if (values.length() !in 1..MAX_ARTIFACTS)
            throw SyncGenerationException("artifact descriptor bound")
        val names = mutableSetOf<String>()
        val paths = mutableSetOf<String>()
        var total = 0L
        for (index in 0 until values.length()) {
            val item = values.getJSONObject(index)
            if (
                item.keys().asSequence().toSet() !=
                    setOf(
                        "logical_name",
                        "format",
                        "version",
                        "filename",
                        "size",
                        "sha256",
                        "required",
                    )
            )
                throw SyncGenerationException("invalid artifact descriptor")
            val name = item.getString("logical_name")
            if (
                !name.all { it.code in 0..127 } ||
                    name.toByteArray().size !in 1..MAX_LOGICAL_NAME_BYTES ||
                    !names.add(name)
            )
                throw SyncGenerationException("invalid logical name")
            val path = item.getString("filename")
            safePath(path, generation)
            if (!paths.add(path)) throw SyncGenerationException("duplicate artifact path")
            if (
                item.opt("version") !is Int ||
                    item.getInt("version") < 1 ||
                    item.opt("size") !is Number ||
                    item.getLong("size") !in 0..MAX_ARTIFACT_BYTES ||
                    item.opt("required") !is Boolean ||
                    !DIGEST.matches(item.getString("sha256"))
            )
                throw SyncGenerationException("invalid artifact metadata")
            if (
                item.getBoolean("required") &&
                    (item.getString("format") to item.getInt("version")) !in SUPPORTED
            )
                throw SyncGenerationException("unsupported required capability")
            total = Math.addExact(total, item.getLong("size"))
            if (total > MAX_GENERATION_BYTES)
                throw SyncGenerationException("generation exceeds bound")
        }
        if (!ANDROID_KINDS.keys.all { it in names })
            throw SyncGenerationException("missing required domain")
        return root
    }

    private data class ValidatedGeneration(
        val manifest: JSONObject,
        val digest: String,
        val artifacts: Map<String, String>,
    )

    private fun validatePublished(directory: File, expectedConsumer: String): ValidatedGeneration {
        if (directory.isDirectory.not() || java.nio.file.Files.isSymbolicLink(directory.toPath()))
            throw SyncGenerationException("generation directory missing or unsafe")
        val marker = File(directory, "manifest.json")
        if (
            !marker.isFile ||
                java.nio.file.Files.isSymbolicLink(marker.toPath()) ||
                marker.length() > MAX_MANIFEST_BYTES
        )
            throw SyncGenerationException("complete manifest marker missing")
        val raw = marker.readBytes()
        val manifest = validateManifest(raw, expectedConsumer)
        val artifacts = linkedMapOf<String, String>()
        var total = 0L
        val values = manifest.getJSONArray("artifacts")
        for (index in 0 until values.length()) {
            val item = values.getJSONObject(index)
            val filename = item.getString("filename").substringAfterLast('/')
            val file = File(directory, filename)
            if (
                !file.isFile ||
                    java.nio.file.Files.isSymbolicLink(file.toPath()) ||
                    file.canonicalFile.parentFile != directory.canonicalFile
            )
                throw SyncGenerationException("missing or unsafe listed artifact")
            val declared = item.getLong("size")
            if (file.length() != declared || declared > MAX_ARTIFACT_BYTES)
                throw SyncGenerationException("artifact size mismatch")
            total = Math.addExact(total, declared)
            if (total > MAX_GENERATION_BYTES)
                throw SyncGenerationException("generation exceeds bound")
            val digest = MessageDigest.getInstance("SHA-256")
            val bytes =
                file.inputStream().use { input ->
                    val output = java.io.ByteArrayOutputStream(declared.toInt())
                    val buffer = ByteArray(1024 * 1024)
                    var count: Int
                    var seen = 0L
                    while (input.read(buffer).also { count = it } >= 0) {
                        if (count == 0) continue
                        seen += count
                        if (seen > declared)
                            throw SyncGenerationException("artifact grew during staging")
                        digest.update(buffer, 0, count)
                        output.write(buffer, 0, count)
                    }
                    output.toByteArray()
                }
            if (digest.digest().joinToString("") { "%02x".format(it) } != item.getString("sha256"))
                throw SyncGenerationException("artifact digest mismatch")
            artifacts[item.getString("logical_name")] = bytes.toString(StandardCharsets.UTF_8)
        }
        return ValidatedGeneration(manifest, sha256(raw), artifacts)
    }

    private fun failure(result: Any): String? =
        when (result) {
            is PcCatalogImportResult.Applied,
            is ExerciseProfileStateImportResult.Applied,
            is EquipmentDefinitionImportResult.Applied,
            is MobileSessionImportResult.Applied,
            is EquipmentAssociationImportResult.Applied,
            is ExerciseBodyZoneImportResult.Applied,
            is TrainingFeedbackImportResult.Applied,
            is ExerciseAliasImportResult.Applied,
            is ExecutionDraftImportResult.Applied,
            is AiSessionDraftImportResult.Applied,
            is CausalDeleteResult.Applied,
            is CausalDeleteResult.Unchanged -> null
            else -> result.toString()
        }

    private fun recordRejected(
        manifest: JSONObject,
        digest: String,
        localPeer: String,
        error: SyncGenerationException,
    ): String =
        repository.inSyncGenerationTransaction { db ->
            val generation = manifest.getString("generation_id")
            db.rawQuery(
                    "SELECT manifest_sha256,ack_json FROM sync_consumed_generations WHERE generation_id=?",
                    arrayOf(generation),
                )
                .use { known ->
                    if (known.moveToFirst()) {
                        if (known.getString(0) != digest)
                            throw SyncGenerationException(
                                "generation identity reused with different manifest"
                            )
                        return@inSyncGenerationTransaction known.getString(1)
                    }
                }
            var diagnostic = error.message ?: "generation rejected"
            while (
                diagnostic.toByteArray(StandardCharsets.UTF_8).size > MAX_DIAGNOSTIC_BYTES
            ) diagnostic = diagnostic.dropLast(1)
            val at = OffsetDateTime.now().toString()
            val producer = manifest.getJSONObject("producer").getString("peer_id")
            val ack =
                JSONObject()
                    .put("format", "trainlog-sync-ack")
                    .put("version", 1)
                    .put("ack_id", "ack_${generation.removePrefix("gen_")}")
                    .put("run_id", manifest.getString("run_id"))
                    .put("generation_id", generation)
                    .put("producer_peer_id", producer)
                    .put("consumer_peer_id", localPeer)
                    .put("manifest_sha256", digest)
                    .put("result", "rejected")
                    .put("durability", "sqlite-commit-rejection")
                    .put("consumed_at", at)
                    .put("diagnostic", diagnostic)
            ack.put("payload_sha256", sha256(canonical(ack).toByteArray(StandardCharsets.UTF_8)))
            val json = canonical(ack)
            db.execSQL(
                "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                arrayOf(
                    generation,
                    manifest.getString("run_id"),
                    producer,
                    localPeer,
                    if (manifest.isNull("parent_generation_id")) null
                    else manifest.getString("parent_generation_id"),
                    digest,
                    at,
                    "rejected",
                    "sqlite-commit-rejection",
                    diagnostic,
                    json,
                ),
            )
            db.execSQL(
                "INSERT INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                arrayOf(
                    ack.getString("ack_id"),
                    generation,
                    manifest.getString("run_id"),
                    producer,
                    localPeer,
                    digest,
                    "rejected",
                    "sqlite-commit-rejection",
                    at,
                    diagnostic,
                    ack.getString("payload_sha256"),
                ),
            )
            json
        }

    /**
     * Validates all bytes before mutation, then applies every listed domain and the durable
     * consumption/ACK record in one outer SQLite transaction.
     */
    fun consume(directory: File): String {
        val localPeer = stablePeerId("android")
        val frozen = validatePublished(directory, localPeer)
        val manifest = frozen.manifest
        val generation = manifest.getString("generation_id")
        return try {
            repository.inSyncGenerationTransaction { db ->
                db.rawQuery(
                        "SELECT manifest_sha256,ack_json FROM sync_consumed_generations WHERE generation_id=?",
                        arrayOf(generation),
                    )
                    .use { known ->
                        if (known.moveToFirst()) {
                            if (known.getString(0) != frozen.digest)
                                throw SyncGenerationException(
                                    "generation identity reused with different manifest"
                                )
                            return@inSyncGenerationTransaction known.getString(1)
                        }
                    }
                val producerPeer = manifest.getJSONObject("producer").getString("peer_id")
                val expectedParent =
                    db.rawQuery(
                            "SELECT g.generation_id FROM sync_consumed_generations g WHERE g.producer_peer_id=? AND g.result='consumed' AND NOT EXISTS(SELECT 1 FROM sync_consumed_generations c WHERE c.parent_generation_id=g.generation_id AND c.result='consumed') LIMIT 2",
                            arrayOf(producerPeer),
                        )
                        .use {
                            if (!it.moveToFirst()) null
                            else {
                                val value = it.getString(0)
                                if (it.moveToNext())
                                    throw SyncGenerationException(
                                        "consumed generation lineage has multiple tips"
                                    )
                                value
                            }
                        }
                val suppliedParent =
                    if (manifest.isNull("parent_generation_id")) null
                    else manifest.getString("parent_generation_id")
                if (suppliedParent != expectedParent)
                    throw SyncGenerationException("generation lineage is stale or unrelated")
                val a = frozen.artifacts
                fun apply(name: String, result: () -> Any) {
                    val json =
                        a[name] ?: throw SyncGenerationException("missing required domain: $name")
                    failure(result())?.let { throw SyncGenerationException("$name rejected: $it") }
                }
                // Tombstones are visible before live snapshots; typed generation
                // imports know the complete causal envelope is present.
                apply("causal-deletions") {
                    repository.applyCausalDeletionExportV1Json(checkNotNull(a["causal-deletions"]))
                }
                apply("catalog") { repository.applyPcCatalogJson(checkNotNull(a["catalog"])) }
                apply("exercise-profile-state") {
                    repository.applyExerciseProfileStateJson(
                        checkNotNull(a["exercise-profile-state"]),
                        allowPending = true,
                    )
                }
                apply("exercise-aliases") {
                    repository.applyExerciseAliasesJson(checkNotNull(a["exercise-aliases"]))
                }
                apply("equipment-definitions") {
                    repository.applyPcEquipmentDefinitionsJson(
                        checkNotNull(a["equipment-definitions"]),
                        true,
                    )
                }
                apply("history") {
                    repository.applyPcMobileExportV4Json(checkNotNull(a["history"]))
                }
                apply("equipment-associations") {
                    repository.applyPcEquipmentAssociationsJson(
                        checkNotNull(a["equipment-associations"])
                    )
                }
                apply("body-zones") {
                    repository.applyExerciseBodyZonesJson(checkNotNull(a["body-zones"]), true)
                }
                apply("feedback") {
                    repository.applyTrainingFeedbackJson(checkNotNull(a["feedback"]), true)
                }
                apply("execution-drafts") {
                    repository.applyExecutionDraftExportV1Json(checkNotNull(a["execution-drafts"]))
                }
                if ("ai-proposals" in a)
                    apply("ai-proposals") {
                        repository.applyAiSessionDraftsJson(checkNotNull(a["ai-proposals"]))
                    }
                if ("session-preparations" in a)
                    apply("session-preparations") {
                        repository.applySessionPreparationsJson(
                            checkNotNull(a["session-preparations"])
                        )
                    }
                val consumedAt = OffsetDateTime.now().toString()
                val ack =
                    JSONObject()
                        .put("format", "trainlog-sync-ack")
                        .put("version", 1)
                        .put("ack_id", "ack_${generation.removePrefix("gen_")}")
                        .put("run_id", manifest.getString("run_id"))
                        .put("generation_id", generation)
                        .put(
                            "producer_peer_id",
                            manifest.getJSONObject("producer").getString("peer_id"),
                        )
                        .put("consumer_peer_id", localPeer)
                        .put("manifest_sha256", frozen.digest)
                        .put("result", "consumed")
                        .put("durability", "sqlite-commit-full")
                        .put("consumed_at", consumedAt)
                        .put("diagnostic", "")
                val payload = canonical(ack)
                ack.put("payload_sha256", sha256(payload.toByteArray(StandardCharsets.UTF_8)))
                val ackJson = canonical(ack)
                db.execSQL(
                    "INSERT INTO sync_consumed_generations VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                    arrayOf(
                        generation,
                        manifest.getString("run_id"),
                        manifest.getJSONObject("producer").getString("peer_id"),
                        localPeer,
                        suppliedParent,
                        frozen.digest,
                        consumedAt,
                        "consumed",
                        "sqlite-commit-full",
                        "",
                        ackJson,
                    ),
                )
                db.execSQL(
                    "INSERT INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                    arrayOf(
                        ack.getString("ack_id"),
                        generation,
                        manifest.getString("run_id"),
                        manifest.getJSONObject("producer").getString("peer_id"),
                        localPeer,
                        frozen.digest,
                        "consumed",
                        "sqlite-commit-full",
                        consumedAt,
                        "",
                        ack.getString("payload_sha256"),
                    ),
                )
                ackJson
            }
        } catch (error: SyncGenerationException) {
            recordRejected(manifest, frozen.digest, localPeer, error)
        }
    }

    fun acceptAcknowledgement(raw: ByteArray): String {
        if (
            raw.size > MAX_MANIFEST_BYTES ||
                !repository.syncGenerationJsonHasUniqueKeys(raw.toString(StandardCharsets.UTF_8))
        )
            throw SyncGenerationException("invalid ACK JSON")
        val ack =
            try {
                JSONObject(raw.toString(StandardCharsets.UTF_8))
            } catch (_: Exception) {
                throw SyncGenerationException("invalid ACK JSON")
            }
        boundedJson(ack)
        val keys =
            setOf(
                "format",
                "version",
                "ack_id",
                "run_id",
                "generation_id",
                "producer_peer_id",
                "consumer_peer_id",
                "manifest_sha256",
                "result",
                "durability",
                "consumed_at",
                "diagnostic",
                "payload_sha256",
            )
        if (
            ack.keys().asSequence().toSet() != keys ||
                ack.optString("format") != "trainlog-sync-ack" ||
                ack.opt("version") !is Int ||
                ack.getInt("version") != 1 ||
                ack.getString("result") !in setOf("consumed", "rejected") ||
                ack.getString("durability") !=
                    (if (ack.getString("result") == "consumed") "sqlite-commit-full"
                    else "sqlite-commit-rejection") ||
                ack.getString("diagnostic").toByteArray().size > MAX_DIAGNOSTIC_BYTES
        )
            throw SyncGenerationException("unsupported ACK")
        requireId(ack.getString("run_id"), "sy")
        requireId(ack.getString("generation_id"), "gen")
        requireId(ack.getString("producer_peer_id"), "peer")
        requireId(ack.getString("consumer_peer_id"), "peer")
        if (
            ack.getString("ack_id") !=
                "ack_${ack.getString("generation_id").removePrefix("gen_")}" ||
                !DIGEST.matches(ack.getString("manifest_sha256"))
        )
            throw SyncGenerationException("invalid ACK identity")
        try {
            OffsetDateTime.parse(ack.getString("consumed_at"))
        } catch (_: Exception) {
            throw SyncGenerationException("invalid ACK timestamp")
        }
        val payload = JSONObject(ack.toString())
        val claimed =
            payload.remove("payload_sha256") as? String
                ?: throw SyncGenerationException("ACK digest missing")
        if (
            !DIGEST.matches(claimed) ||
                sha256(canonical(payload).toByteArray(StandardCharsets.UTF_8)) != claimed
        )
            throw SyncGenerationException("ACK digest mismatch")
        return repository.inSyncGenerationTransaction { db ->
            db.rawQuery(
                    "SELECT run_id,producer_peer_id,consumer_peer_id,manifest_sha256,status FROM sync_generations WHERE generation_id=?",
                    arrayOf(ack.getString("generation_id")),
                )
                .use { row ->
                    if (!row.moveToFirst())
                        throw SyncGenerationException("ACK has no pending generation")
                    if (
                        ack.getString("run_id") != row.getString(0) ||
                            ack.getString("producer_peer_id") != row.getString(1) ||
                            ack.getString("consumer_peer_id") != row.getString(2) ||
                            ack.getString("manifest_sha256") != row.getString(3)
                    )
                        throw SyncGenerationException("ACK does not match pending generation")
                    val target =
                        if (ack.getString("result") == "consumed") "acknowledged" else "rejected"
                    /* CONTRACT: the correlated ACK is permanent causal
                     * evidence and is committed atomically with generation
                     * state. Replay is byte-identical or rejected below. */
                    db.execSQL(
                        "INSERT OR IGNORE INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                        arrayOf(
                            ack.getString("ack_id"),
                            ack.getString("generation_id"),
                            ack.getString("run_id"),
                            ack.getString("producer_peer_id"),
                            ack.getString("consumer_peer_id"),
                            ack.getString("manifest_sha256"),
                            ack.getString("result"),
                            ack.getString("durability"),
                            ack.getString("consumed_at"),
                            ack.getString("diagnostic"),
                            claimed,
                        ),
                    )
                    db.rawQuery(
                        "SELECT payload_sha256 FROM sync_acknowledgements WHERE ack_id=?",
                        arrayOf(ack.getString("ack_id")),
                    ).use { stored ->
                        if (!stored.moveToFirst() || stored.getString(0) != claimed)
                            throw SyncGenerationException("conflicting ACK replay")
                    }
                    if (row.getString(4) in setOf("acknowledged", "rejected")) {
                        if (row.getString(4) != target)
                            throw SyncGenerationException("conflicting ACK replay")
                        return@inSyncGenerationTransaction "unchanged"
                    }
                    if (row.getString(4) != "waiting_acknowledgement")
                        throw SyncGenerationException("generation is not awaiting ACK")
                    db.execSQL(
                        "UPDATE sync_generations SET status=?,acknowledged_at=? WHERE generation_id=?",
                        arrayOf(
                            target,
                            ack.getString("consumed_at"),
                            ack.getString("generation_id"),
                        ),
                    )
                    target
                }
        }
    }
}
