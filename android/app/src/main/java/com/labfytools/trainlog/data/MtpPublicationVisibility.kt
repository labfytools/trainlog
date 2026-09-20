package com.labfytools.trainlog.data

import android.content.Context
import android.media.MediaScannerConnection
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Confirms that durable external-storage files have reached Android's MTP index. */
internal fun interface MtpPublicationVisibility {
    fun confirm(files: List<File>)
}

internal object ImmediateMtpPublicationVisibility : MtpPublicationVisibility {
    override fun confirm(files: List<File>) = Unit
}

internal class AndroidMtpPublicationVisibility(context: Context) : MtpPublicationVisibility {
    private val applicationContext = context.applicationContext

    override fun confirm(files: List<File>) {
        val paths = files.asSequence().filter(File::isFile).map { it.absolutePath }.distinct().toList()
        if (paths.isEmpty()) return
        val completed = CountDownLatch(paths.size)
        /* WHY: direct File writes are durable before MediaProvider necessarily
         * advertises them through Samsung's MTP database. CONTRACT: every
         * immutable artifact is scanned before its mutable reference, and a
         * publication is not reported complete until every callback arrives.
         * INVARIANT: scanning changes transport visibility only; canonical
         * names, bytes, generation identities and commit markers are intact. */
        MediaScannerConnection.scanFile(
            applicationContext,
            paths.toTypedArray(),
            Array(paths.size) { "application/json" },
        ) { _, _ -> completed.countDown() }
        if (!completed.await(15, TimeUnit.SECONDS)) {
            throw SyncGenerationException("timeout confirming MTP publication visibility")
        }
    }
}
