plugins {
    id("com.android.library")
}

android {
    namespace = "dev.danielc.libpak"
    compileSdk = 37

    defaultConfig {
        minSdk = 24
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
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
    implementation(libs.androidx.annotation.jvm)
    testImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.junit.v115)
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
}