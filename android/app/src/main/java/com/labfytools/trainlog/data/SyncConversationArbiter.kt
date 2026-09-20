package com.labfytools.trainlog.data

import java.time.Duration
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

internal enum class SyncConversationOwner {
    FOREGROUND_EXPLICIT,
    BACKGROUND_AUTOMATIC,
}

internal class SyncConversationYieldedException : Exception("conversation superseded by explicit sync")

/** Process-wide ownership authority for Android generation conversations. */
internal class SyncConversationArbiter {
    private val lock = ReentrantLock()
    private val ownershipChanged = lock.newCondition()
    private var active: Lease? = null
    private var pendingExplicit = 0

    /**
     * Announces user intent before the foreground service is refreshed.
     *
     * WHY: starting the service first allowed it to acquire the old binary
     * lock and silently absorb the click. CONTRACT: an announcement prevents
     * new background ownership and cooperatively supersedes an existing
     * background lease. INVARIANT: ownership is still exclusive until that
     * lease observes yield and releases in its finally block.
     */
    fun announceExplicit(): ExplicitClaim = lock.withLock {
        pendingExplicit += 1
        active?.takeIf { it.owner == SyncConversationOwner.BACKGROUND_AUTOMATIC }
            ?.requestYield()
        ownershipChanged.signalAll()
        ExplicitClaim(this)
    }

    fun tryAcquireBackground(): Lease? = lock.withLock {
        if (active != null || pendingExplicit > 0) return null
        Lease(this, SyncConversationOwner.BACKGROUND_AUTOMATIC).also { active = it }
    }

    private fun acquireExplicit(claim: ExplicitClaim, timeout: Duration): Lease? {
        return lock.withLock {
            check(claim.announced)
            var remaining = timeout.toNanos()
            while (active != null) {
                if (remaining <= 0L) {
                    releaseAnnouncement(claim)
                    return@withLock null
                }
                remaining = ownershipChanged.awaitNanos(remaining)
            }
            releaseAnnouncement(claim)
            Lease(this, SyncConversationOwner.FOREGROUND_EXPLICIT).also { active = it }
        }
    }

    private fun releaseAnnouncement(claim: ExplicitClaim) {
        if (!claim.announced) return
        claim.announced = false
        pendingExplicit -= 1
        check(pendingExplicit >= 0)
        ownershipChanged.signalAll()
    }

    private fun release(lease: Lease) = lock.withLock {
        if (active === lease) {
            active = null
            ownershipChanged.signalAll()
        }
    }

    internal class ExplicitClaim internal constructor(private val arbiter: SyncConversationArbiter) :
        AutoCloseable {
        internal var announced = true
        private var acquired = false

        fun acquire(timeout: Duration): Lease? {
            check(!acquired)
            val lease = arbiter.acquireExplicit(this, timeout)
            acquired = lease != null
            return lease
        }

        override fun close() = arbiter.lock.withLock {
            if (!acquired) arbiter.releaseAnnouncement(this)
        }
    }

    internal class Lease internal constructor(
        private val arbiter: SyncConversationArbiter,
        val owner: SyncConversationOwner,
    ) : AutoCloseable {
        @Volatile private var yieldRequested = false
        @Volatile private var closed = false

        internal fun requestYield() {
            yieldRequested = true
        }

        fun throwIfYieldRequested() {
            if (yieldRequested) throw SyncConversationYieldedException()
        }

        override fun close() {
            if (closed) return
            closed = true
            arbiter.release(this)
        }
    }

    companion object {
        val process = SyncConversationArbiter()
    }
}
