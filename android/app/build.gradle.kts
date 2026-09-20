import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

/*
 * CONTRACT: release-signing material is machine-local. Environment variables
 * take precedence over the private properties file so CI and release hosts can
 * inject secrets without copying them into the source tree.
 */
val releaseSigningProperties = Properties()
val releaseSigningPropertiesPath =
    providers.environmentVariable("TRAINLOG_RELEASE_CREDENTIALS_FILE").orNull
        ?: "${System.getProperty("user.home")}/.config/trainlog/release-signing.properties"
val releaseSigningPropertiesFile = file(releaseSigningPropertiesPath)
if (releaseSigningPropertiesFile.isFile) {
    releaseSigningPropertiesFile.inputStream().use(releaseSigningProperties::load)
}

fun releaseSigningValue(environmentName: String, propertyName: String): String? =
    providers.environmentVariable(environmentName).orNull
        ?.takeIf { it.isNotBlank() }
        ?: releaseSigningProperties.getProperty(propertyName)?.takeIf { it.isNotBlank() }

val releaseStoreFile = releaseSigningValue("TRAINLOG_RELEASE_STORE_FILE", "storeFile")
val releaseStorePassword = releaseSigningValue("TRAINLOG_RELEASE_STORE_PASSWORD", "storePassword")
val releaseKeyAlias = releaseSigningValue("TRAINLOG_RELEASE_KEY_ALIAS", "keyAlias")
val releaseKeyPassword = releaseSigningValue("TRAINLOG_RELEASE_KEY_PASSWORD", "keyPassword")
val releaseSigningComplete = listOf(
    releaseStoreFile,
    releaseStorePassword,
    releaseKeyAlias,
    releaseKeyPassword,
).all { !it.isNullOrBlank() }

/* CONTRACT: a private rollout may need an update-only version code above the
 * repository default while preserving versionName, schemas and protocols.
 * INVARIANT: the override remains a positive Android package version code and
 * never changes the source default used by ordinary builds. */
val trainlogVersionCode =
    providers.environmentVariable("TRAINLOG_ANDROID_VERSION_CODE").orNull?.let { raw ->
        raw.toIntOrNull()?.takeIf { it in 1..Int.MAX_VALUE }
            ?: throw GradleException("TRAINLOG_ANDROID_VERSION_CODE must be a positive integer")
    } ?: 5

android {
    namespace = "com.labfytools.trainlog"
    compileSdk = 37

    defaultConfig {
        applicationId = "com.labfytools.trainlog"
        minSdk = 26
        targetSdk = 36

        versionCode = trainlogVersionCode
        versionName = "0.1.4"
        testInstrumentationRunner =
            "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        if (releaseSigningComplete) {
            create("release") {
                storeFile = file(requireNotNull(releaseStoreFile))
                storePassword = requireNotNull(releaseStorePassword)
                keyAlias = requireNotNull(releaseKeyAlias)
                keyPassword = requireNotNull(releaseKeyPassword)
            }
        }
    }

    buildTypes {
        getByName("release") {
            if (releaseSigningComplete) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }

    sourceSets {
        getByName("main") {
            /* CONTRACT: repository-level equipment and body-zone manifests
             * are the canonical shared sources. Android must not fork either
             * taxonomy into Kotlin constants. */
            assets.directories.add("../../catalog")
            /* Shared production golden inputs are read by Kotlin directly and
             * generated into the C runner; neither platform authors copies. */
            assets.directories.add("../../tests/fixtures")
        }
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
        unitTests.all {
            it.systemProperty(
                "user.home",
                layout.buildDirectory
                    .get()
                    .asFile
                    .absolutePath,
            )
        }
    }
}

/* WHY: Android Gradle otherwise emits app-release-unsigned.apk successfully,
 * which is build evidence but can be mistaken for a publishable artifact.
 * INVARIANT: every task that packages a release fails before packaging unless
 * all four external values exist and the configured keystore is a regular
 * file. Gradle/apksigner still perform the cryptographic validation. */
tasks.matching { it.name == "packageRelease" }.configureEach {
    doFirst {
        if (!releaseSigningComplete) {
            throw GradleException(
                "Trainlog release signing is not configured. Set the four " +
                    "TRAINLOG_RELEASE_* variables or provide the private " +
                    "release-signing.properties file.",
            )
        }
        val configuredStore = file(requireNotNull(releaseStoreFile))
        if (!configuredStore.isFile) {
            throw GradleException("Trainlog release keystore does not exist: $configuredStore")
        }
    }
}

dependencies {
    val composeBom =
        platform("androidx.compose:compose-bom:2026.08.00")

    implementation(composeBom)
    androidTestImplementation(composeBom)
    testImplementation(composeBom)

    implementation(
        "androidx.activity:activity-compose:1.13.0"
    )
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.documentfile:documentfile:1.1.0")
    implementation("androidx.work:work-runtime-ktx:2.10.5")

    implementation(
        "androidx.compose.foundation:foundation"
    )

    implementation("androidx.compose.material3:material3")
    implementation(
        "androidx.compose.ui:ui"
    )

    implementation(
        "androidx.compose.ui:ui-tooling-preview"
    )

    debugImplementation(
        "androidx.compose.ui:ui-tooling"
    )
    debugImplementation("androidx.compose.ui:ui-test-manifest")

    testImplementation("junit:junit:4.13.2")
    testImplementation("androidx.test:core:1.7.0")
    testImplementation("org.robolectric:robolectric:4.16.1")
    testImplementation("androidx.compose.ui:ui-test-junit4")

    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
}
