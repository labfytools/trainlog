package com.labfytools.trainlog.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.ActiveDraftLoadResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.ui.theme.TrainlogTheme

const val DRAFT_UI_TEST_DATABASE_NAME =
    "trainlog-draft-ui-test.db"

/**
 * Debug-only instrumentation host. It deliberately renders production screens
 * without SyncExporter and can only open the fixed isolated test database.
 */
class DraftUiTestActivity : ComponentActivity() {
    private lateinit var repository: TrainlogRepository

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        repository =
            TrainlogRepository(
                applicationContext,
                DRAFT_UI_TEST_DATABASE_NAME,
            )

        setContent {
            TrainlogTheme {
                var sessionVisible by
                    remember {
                        mutableStateOf(false)
                    }
                var revision by
                    remember {
                        mutableIntStateOf(0)
                    }
                var error by
                    remember {
                        mutableStateOf<String?>(null)
                    }

                if (sessionVisible) {
                    SessionScreen(
                        repository = repository,
                        catalogRevision = 0,
                        onBack = {
                            revision += 1
                            sessionVisible = false
                        },
                        onCreateExercise = {},
                        /* CONTRACT: this host never writes shared export data. */
                        onSessionSaved = {},
                    )
                } else {
                    val loaded =
                        remember(revision) {
                            repository
                                .loadActiveSessionDraft()
                        }
                    HomeScreen(
                        activeDraft =
                            (loaded as?
                                ActiveDraftLoadResult.Loaded)
                                ?.draft,
                        draftError =
                            error
                                ?: (loaded as?
                                    ActiveDraftLoadResult.Error)
                                    ?.message
                                ?: (loaded as?
                                    ActiveDraftLoadResult.Loaded)
                                    ?.warning,
                        onSession = {
                            when (
                                val result =
                                    repository
                                        .startActiveSessionDraft()
                            ) {
                                ActiveDraftMutationResult.Saved -> {
                                    error = null
                                    sessionVisible = true
                                }

                                is ActiveDraftMutationResult.Error -> {
                                    error = result.message
                                }
                            }
                        },
                        onDiscardDraft = {
                            when (
                                val result =
                                    repository
                                        .discardActiveSessionDraft()
                            ) {
                                ActiveDraftMutationResult.Saved -> {
                                    error = null
                                    revision += 1
                                }

                                is ActiveDraftMutationResult.Error -> {
                                    error = result.message
                                }
                            }
                        },
                        onExercise = {},
                        onBody = {},
                        onHistory = {},
                        onSync = {},
                        onGenerateSession = {},
                    )
                }
            }
        }
    }

    override fun onDestroy() {
        repository.close()
        super.onDestroy()
    }
}
