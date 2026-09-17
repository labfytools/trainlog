package com.labfytools.trainlog.data

import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.isDirectory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class IsolatedTestEnvironmentTest {
    @Test
    fun `private paths reach the Robolectric JVM`() {
        val rootText = System.getenv("TRAINLOG_TEST_RUN_ROOT") ?: return
        val root = Path(rootText).toRealPath()
        fun assertedPath(environmentName: String): Path {
            val value = System.getenv(environmentName)
            assertTrue("$environmentName missing", !value.isNullOrBlank())
            val path = Path(value!!).toRealPath()
            assertTrue("$environmentName escapes run root", path.startsWith(root))
            assertTrue("$environmentName is not a directory", path.isDirectory())
            return path
        }

        /* CONTRACT: JAVA_TOOL_OPTIONS must reach the forked test JVM, not only
         * the Gradle launcher; native Robolectric extraction uses this value. */
        assertEquals(assertedPath("TRAINLOG_TEST_TMPDIR"), Path(System.getProperty("java.io.tmpdir")).toRealPath())
        assertedPath("HOME")
        assertedPath("XDG_DATA_HOME")
        assertedPath("XDG_CONFIG_HOME")
        assertedPath("XDG_CACHE_HOME")
        assertedPath("XDG_RUNTIME_DIR")
        assertedPath("TRAINLOG_TEST_EXCHANGE_DIR")
        assertedPath("TRAINLOG_TEST_DATABASE_DIR")
    }
}
