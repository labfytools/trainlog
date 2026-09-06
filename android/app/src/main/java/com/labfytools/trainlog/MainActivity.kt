package com.labfytools.trainlog

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.ui.TrainlogApp
import com.labfytools.trainlog.ui.theme.TrainlogTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(
        savedInstanceState: Bundle?
    ) {
        super.onCreate(savedInstanceState)

        val repository =
            TrainlogRepository(
                applicationContext
            )

        val exporter =
            SyncExporter(
                applicationContext,
                repository,
            )

        val inbox =
            SyncCatalogInbox(
                applicationContext,
                repository,
            )

        val requestOutbox =
            SyncRequestOutbox(
                applicationContext
            )

        setContent {
            TrainlogTheme {
                TrainlogApp(
                    repository = repository,
                    exporter = exporter,
                    inbox = inbox,
                    requestOutbox =
                        requestOutbox,
                )
            }
        }
    }
}
