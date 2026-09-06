plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.labfytools.trainlog"
    compileSdk = 37

    defaultConfig {
        applicationId = "com.labfytools.trainlog"
        minSdk = 26
        targetSdk = 36

        versionCode = 1
        versionName = "0.1.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }
}

dependencies {
    val composeBom =
        platform("androidx.compose:compose-bom:2026.08.00")

    implementation(composeBom)

    implementation(
        "androidx.activity:activity-compose:1.13.0"
    )

    implementation(
        "androidx.compose.foundation:foundation"
    )

    implementation(
        "androidx.compose.ui:ui"
    )

    implementation(
        "androidx.compose.ui:ui-tooling-preview"
    )

    debugImplementation(
        "androidx.compose.ui:ui-tooling"
    )
}
