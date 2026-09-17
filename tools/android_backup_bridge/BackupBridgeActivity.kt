package com.labfytools.trainlog.data

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/** Minimal non-migrating bridge UI; it intentionally exposes backup/restore only. */
class BackupBridgeActivity : Activity() {
    private lateinit var repository: TrainlogRepository
    private lateinit var status: TextView
    private val service by lazy { AndroidBackupService(applicationContext) }

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        repository = TrainlogRepository(applicationContext)
        status =
            TextView(this).apply {
                text = "Plaintext local backup. Keep the exported file private."
            }
        val backup =
            Button(this).apply {
                text = "Create complete backup"
                setOnClickListener {
                    startActivityForResult(
                        Intent(Intent.ACTION_CREATE_DOCUMENT)
                            .setType("application/zip")
                            .putExtra(Intent.EXTRA_TITLE, "trainlog-backup.tlbackup")
                            .addCategory(Intent.CATEGORY_OPENABLE),
                        1,
                    )
                }
            }
        val restore =
            Button(this).apply {
                text = "Verify and restore backup"
                setOnClickListener {
                    startActivityForResult(
                        Intent(Intent.ACTION_OPEN_DOCUMENT)
                            .setType("application/zip")
                            .addCategory(Intent.CATEGORY_OPENABLE),
                        2,
                    )
                }
            }
        setContentView(
            LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                addView(status)
                addView(backup)
                addView(restore)
            }
        )
    }

    override fun onActivityResult(request: Int, result: Int, data: Intent?) {
        super.onActivityResult(request, result, data)
        if (result != RESULT_OK || data?.data == null) return
        if (request == 1)
            contentResolver.openOutputStream(data.data!!, "w")!!.use {
                status.text = service.create(repository, it).toString()
            }
        else if (request == 2)
            contentResolver.openInputStream(data.data!!)!!.use {
                val verified =
                    service.verify(it).getOrElse { error ->
                        status.text = error.message
                        return
                    }
                status.text = service.restore(repository, verified).toString()
                recreate()
            }
    }

    override fun onDestroy() {
        repository.close()
        super.onDestroy()
    }
}
