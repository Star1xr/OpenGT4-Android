// OpenGT4 Android - kok build dosyasi. Surumler gradle/libs.versions.toml'da.
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
}
