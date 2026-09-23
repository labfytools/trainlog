package com.labfytools.trainlog.data

import com.labfytools.trainlog.model.CardioGuidanceInstruction
import com.labfytools.trainlog.model.CardioPhaseKind
import com.labfytools.trainlog.model.CardioTargetSnapshot
import java.util.concurrent.CopyOnWriteArraySet

internal data class CardioGuidanceLiveSnapshot(
    val phaseId: String? = null,
    val phaseKind: CardioPhaseKind? = null,
    val instruction: CardioGuidanceInstruction = CardioGuidanceInstruction.SUSPENDED,
    val bpm: Int? = null,
    val target: CardioTargetSnapshot? = null,
    val startedAt: String? = null,
)

internal object CardioGuidanceLiveState {
    private val listeners =
        CopyOnWriteArraySet<(CardioGuidanceLiveSnapshot) -> Unit>()

    @Volatile
    private var snapshot = CardioGuidanceLiveSnapshot()

    fun current(): CardioGuidanceLiveSnapshot = snapshot

    fun publish(next: CardioGuidanceLiveSnapshot) {
        snapshot = next
        listeners.forEach { it(next) }
    }

    fun clear() {
        publish(CardioGuidanceLiveSnapshot())
    }

    fun subscribe(listener: (CardioGuidanceLiveSnapshot) -> Unit): AutoCloseable {
        listeners += listener
        listener(snapshot)
        return AutoCloseable { listeners -= listener }
    }
}
