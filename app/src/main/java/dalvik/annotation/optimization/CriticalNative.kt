package dalvik.annotation.optimization

/** Compile stub matching ART's hidden runtime annotation; required for no-JNIEnv ABI. */
@Retention(AnnotationRetention.RUNTIME)
@Target(AnnotationTarget.FUNCTION)
annotation class CriticalNative
