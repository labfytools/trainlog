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
        testInstrumentationRunner =
            "androidx.test.runner.AndroidJUnitRunner"
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

dependencies {
    val composeBom =
        platform("androidx.compose:compose-bom:2026.08.00")

    implementation(composeBom)
    androidTestImplementation(composeBom)
    testImplementation(composeBom)

    implementation(
        "androidx.activity:activity-compose:1.13.0"
    )

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

    implementation("androidx.documentfile:documentfile:1.1.0")

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
