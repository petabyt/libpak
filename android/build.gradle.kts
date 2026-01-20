plugins {
    id("com.android.library")
}

android {
    namespace = "dev.danielc.libpak"
    compileSdk = 36

    defaultConfig {
        minSdk = 24
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
    if (!rootProject.extra.has("noNativeModule")) {
        externalNativeBuild {
            cmake {
                path = file("../CMakeLists.txt")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
}