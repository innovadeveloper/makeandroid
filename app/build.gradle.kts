plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.innova.gstream"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.innova.gstream"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        // Configuración CMake para GStreamer
        externalNativeBuild {
            cmake {
                val gstRoot = if (project.hasProperty("gstAndroidRoot")) {
                    project.property("gstAndroidRoot").toString()
                } else {
                    System.getenv("GSTREAMER_ROOT_ANDROID")
                }

                if (gstRoot == null) {
                    throw GradleException("GSTREAMER_ROOT_ANDROID must be set, or 'gstAndroidRoot' must be defined in gradle.properties")
                }

                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DGSTREAMER_ROOT_ANDROID=$gstRoot",
                    "-GNinja"
                )

                targets("gstreamer-native")

                // Arquitecturas soportadas por GStreamer
//                abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
                abiFilters += listOf("arm64-v8a")
            }
        }
    }
    ndkVersion = "25.2.9519653"
    // Configuración externa de CMake
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
    buildFeatures {
        compose = true
    }

    ndkVersion = "25.2.9519653"
}

//afterEvaluate {
//    tasks.findByName("compileDebugJavaWithJavac")?.dependsOn("externalNativeBuildDebug")
//    tasks.findByName("compileReleaseJavaWithJavac")?.dependsOn("externalNativeBuildRelease")
//}


dependencies {

//    implementation(project(":nativelibrary"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)
}