// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
#include "osd/android_platform.h"

#include "core/log.h"

#include <SDL3/SDL.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sm2::osd {
namespace {

#ifndef SM2_VERSION
#define SM2_VERSION "0.9.63"
#endif

constexpr const char* kAssetsStamp = SM2_VERSION "-keisho-1";

std::string g_assets_dir;

bool copy_asset_file(AAssetManager* mgr, const char* asset_path,
                     const std::filesystem::path& dst_path)
{
    AAsset* asset = AAssetManager_open(mgr, asset_path, AASSET_MODE_STREAMING);
    if (asset == nullptr) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(dst_path.parent_path(), ec);

    std::ofstream out(dst_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        AAsset_close(asset);
        return false;
    }

    char buf[16384];
    int  read_bytes = 0;
    while ((read_bytes = AAsset_read(asset, buf, sizeof(buf))) > 0) {
        out.write(buf, read_bytes);
    }
    AAsset_close(asset);
    return out.good();
}

void copy_asset_dir(AAssetManager* mgr, const char* asset_dir,
                    const std::filesystem::path& dst_dir)
{
    AAssetDir* dir = AAssetManager_openDir(mgr, asset_dir);
    if (dir == nullptr) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(dst_dir, ec);
    while (const char* name = AAssetDir_getNextFileName(dir)) {
        const std::string src = std::string(asset_dir) + "/" + name;
        copy_asset_file(mgr, src.c_str(), dst_dir / name);
    }
    AAssetDir_close(dir);
}

jclass find_activity_class(JNIEnv* env, jobject activity)
{
    if (env == nullptr || activity == nullptr) {
        return nullptr;
    }
    return env->GetObjectClass(activity);
}

}  // namespace

std::string android_external_root_dir()
{
    if (const char* ext = SDL_GetAndroidExternalStoragePath(); ext != nullptr && ext[0] != '\0') {
        return ext;
    }
    if (const char* intl = SDL_GetAndroidInternalStoragePath(); intl != nullptr && intl[0] != '\0') {
        return intl;
    }
    return ".";
}

std::string android_shared_storage_dir()
{
    return "/storage/emulated/0";
}

std::string android_assets_dir()
{
    if (!g_assets_dir.empty()) {
        return g_assets_dir;
    }
    if (const char* intl = SDL_GetAndroidInternalStoragePath(); intl != nullptr && intl[0] != '\0') {
        return (std::filesystem::path(intl) / "keisho_assets").string();
    }
    return {};
}

std::string android_prepare_assets()
{
    const std::string dir_str = android_assets_dir();
    if (dir_str.empty()) {
        return {};
    }
    g_assets_dir = dir_str;

    const std::filesystem::path dst_root(dir_str);
    const std::filesystem::path stamp_file = dst_root / ".assets_version";
    std::error_code ec;
    if (std::filesystem::exists(dst_root / "games.xml", ec)
        && std::filesystem::exists(stamp_file, ec)) {
        std::ifstream in(stamp_file);
        std::string   ver;
        std::getline(in, ver);
        if (ver == kAssetsStamp) {
            return dir_str;
        }
    }

    auto* env      = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto  activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        SM2_ERROR("android: could not access JNIEnv/Activity to unpack assets");
        return {};
    }

    jclass act_cls = env->GetObjectClass(activity);
    jmethodID get_assets = env->GetMethodID(act_cls, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject asset_mgr_obj = get_assets != nullptr ? env->CallObjectMethod(activity, get_assets) : nullptr;
    AAssetManager* mgr = asset_mgr_obj != nullptr ? AAssetManager_fromJava(env, asset_mgr_obj) : nullptr;

    if (mgr != nullptr) {
        std::filesystem::create_directories(dst_root, ec);
        copy_asset_file(mgr, "games.xml", dst_root / "games.xml");
        copy_asset_dir(mgr, "lang", dst_root / "lang");
        copy_asset_dir(mgr, "lang/fonts", dst_root / "lang" / "fonts");

        std::ofstream out(stamp_file, std::ios::trunc);
        out << kAssetsStamp << "\n";
        SM2_INFO("android: unpacked assets into %s", dir_str.c_str());
    } else {
        SM2_ERROR("android: AAssetManager_fromJava failed");
    }

    if (asset_mgr_obj != nullptr) {
        env->DeleteLocalRef(asset_mgr_obj);
    }
    env->DeleteLocalRef(act_cls);
    env->DeleteLocalRef(activity);
    return dir_str;
}

bool android_has_storage_access()
{
    auto* env      = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto  activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        return false;
    }
    jclass cls = find_activity_class(env, activity);
    jmethodID mid = cls != nullptr ? env->GetStaticMethodID(cls, "hasStorageAccess", "()Z") : nullptr;
    bool granted = false;
    if (mid != nullptr) {
        granted = env->CallStaticBooleanMethod(cls, mid) == JNI_TRUE;
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (cls != nullptr) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return granted;
}

void android_request_storage_access()
{
    auto* env      = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto  activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }
    jclass cls = find_activity_class(env, activity);
    jmethodID mid = cls != nullptr ? env->GetStaticMethodID(cls, "requestStorageAccess", "()V") : nullptr;
    if (mid != nullptr) {
        env->CallStaticVoidMethod(cls, mid);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (cls != nullptr) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

}  // namespace sm2::osd
