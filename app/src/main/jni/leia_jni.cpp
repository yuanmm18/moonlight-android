#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <arm_neon.h>
#include <dlfcn.h>
#include <math.h>
#include <string.h>

#define TAG "LeiaJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

typedef void (*leiaSet3DOn_func)(int mode);
typedef void (*leiaSet3DOff_func)();

static leiaSet3DOn_func leiaSet3DOn = nullptr;
static leiaSet3DOff_func leiaSet3DOff = nullptr;
static void* leiaLibHandle = nullptr;
static bool current3DState = false;

static bool loadLeiaLibrary() {
    if (leiaLibHandle != nullptr) return true;
    leiaLibHandle = dlopen("liblibleia.so", RTLD_LAZY);
    if (!leiaLibHandle) { LOGE("Failed to load Leia library: %s", dlerror()); return false; }
    leiaSet3DOn = (leiaSet3DOn_func)dlsym(leiaLibHandle, "leiaSet3DOn");
    leiaSet3DOff = (leiaSet3DOff_func)dlsym(leiaLibHandle, "leiaSet3DOff");
    if (!leiaSet3DOn || !leiaSet3DOff) { LOGE("Failed to get Leia function pointers"); dlclose(leiaLibHandle); leiaLibHandle = nullptr; return false; }
    LOGI("Leia library loaded successfully"); return true;
}

static float calculateSAD_NEON(const uint8_t* left, const uint8_t* right, int width, int height) {
    int totalPixels = width * height; int totalSAD = 0; int simdPixels = totalPixels & ~63;
    for (int i = 0; i < simdPixels; i += 64) {
        uint8x16_t left1 = vld1q_u8(left + i), right1 = vld1q_u8(right + i), diff1 = vabdq_u8(left1, right1);
        uint8x16_t left2 = vld1q_u8(left + i + 16), right2 = vld1q_u8(right + i + 16), diff2 = vabdq_u8(left2, right2);
        uint8x16_t left3 = vld1q_u8(left + i + 32), right3 = vld1q_u8(right + i + 32), diff3 = vabdq_u8(left3, right3);
        uint8x16_t left4 = vld1q_u8(left + i + 48), right4 = vld1q_u8(right + i + 48), diff4 = vabdq_u8(left4, right4);
        uint16x8_t sum1 = vpaddlq_u8(diff1), sum2 = vpaddlq_u8(diff2), sum3 = vpaddlq_u8(diff3), sum4 = vpaddlq_u8(diff4);
        uint16x8_t total_sum = vaddq_u16(vaddq_u16(sum1, sum2), vaddq_u16(sum3, sum4));
        uint32x4_t final_sum = vpaddlq_u16(total_sum); uint64x2_t final_sum64 = vpaddlq_u32(final_sum);
        totalSAD += vgetq_lane_s64(vreinterpretq_s64_u64(final_sum64), 0);
    }
    for (int i = simdPixels; i < totalPixels; i++) totalSAD += abs(left[i] - right[i]);
    return (float)totalSAD / (255.0f * totalPixels);
}

static void downsampleToGrayScale(AndroidBitmapInfo* leftInfo, AndroidBitmapInfo* rightInfo,
                                 const uint8_t* leftPixels, const uint8_t* rightPixels,
                                 uint8_t* leftGray, uint8_t* rightGray,
                                 int targetWidth, int targetHeight) {
    float scaleX = (float)leftInfo->width / targetWidth, scaleY = (float)leftInfo->height / targetHeight;
    for (int y = 0; y < targetHeight; y++) {
        for (int x = 0; x < targetWidth; x++) {
            int srcX = (int)(x * scaleX), srcY = (int)(y * scaleY);
            int leftOffset = (srcY * leftInfo->width + srcX) * 4;
            int rightOffset = (srcY * rightInfo->width + srcX) * 4;
            uint8_t leftR = leftPixels[leftOffset], leftG = leftPixels[leftOffset + 1], leftB = leftPixels[leftOffset + 2];
            leftGray[y * targetWidth + x] = (uint8_t)(0.299f * leftR + 0.587f * leftG + 0.114f * leftB);
            uint8_t rightR = rightPixels[rightOffset], rightG = rightPixels[rightOffset + 1], rightB = rightPixels[rightOffset + 2];
            rightGray[y * targetWidth + x] = (uint8_t)(0.299f * rightR + 0.587f * rightG + 0.114f * rightB);
        }
    }
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_limelight_LeiaHelper_isSBS(JNIEnv *env, jobject obj, jobject leftHalf, jobject rightHalf) {
    AndroidBitmapInfo leftInfo, rightInfo; uint8_t* leftPixels = nullptr; uint8_t* rightPixels = nullptr; jboolean result = JNI_FALSE;
    if (AndroidBitmap_getInfo(env, leftHalf, &leftInfo) != ANDROID_BITMAP_RESULT_SUCCESS ||
        AndroidBitmap_getInfo(env, rightHalf, &rightInfo) != ANDROID_BITMAP_RESULT_SUCCESS) { LOGE("Failed to get bitmap info"); return JNI_FALSE; }
    if (AndroidBitmap_lockPixels(env, leftHalf, (void**)&leftPixels) != ANDROID_BITMAP_RESULT_SUCCESS ||
        AndroidBitmap_lockPixels(env, rightHalf, (void**)&rightPixels) != ANDROID_BITMAP_RESULT_SUCCESS) { LOGE("Failed to lock bitmap pixels"); return JNI_FALSE; }
    const int targetWidth = 64, targetHeight = 64;
    uint8_t* leftGray = new uint8_t[targetWidth * targetHeight];
    uint8_t* rightGray = new uint8_t[targetWidth * targetHeight];
    downsampleToGrayScale(&leftInfo, &rightInfo, leftPixels, rightPixels, leftGray, rightGray, targetWidth, targetHeight);
    float sadValue = calculateSAD_NEON(leftGray, rightGray, targetWidth, targetHeight);
    float correlation = 1.0f - sadValue;
    result = (correlation < 0.95f) ? JNI_TRUE : JNI_FALSE;
    LOGI("SBS detection: correlation=%.3f, threshold=0.950, result=%s", correlation, result ? "SBS" : "2D");
    delete[] leftGray; delete[] rightGray;
    AndroidBitmap_unlockPixels(env, leftHalf); AndroidBitmap_unlockPixels(env, rightHalf);
    return result;
}

JNIEXPORT void JNICALL
Java_com_limelight_LeiaHelper_set3D(JNIEnv *env, jobject obj, jboolean on, jint mode) {
    if (!loadLeiaLibrary()) { LOGE("Leia library not available"); return; }
    if (on && current3DState != on) { leiaSet3DOn(mode); current3DState = true; LOGI("Leia: 3D ON (mode=%d)", mode); }
    else if (!on && current3DState != on) { leiaSet3DOff(); current3DState = false; LOGI("Leia: 3D OFF"); }
}

} // extern "C"
