package com.labfytools.trainlog

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.labfytools.trainlog.data.TrainlogRepository
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

        setContent {
            TrainlogTheme {
                TrainlogApp(
                    repository = repository
                )
            }
        }
    }
}
