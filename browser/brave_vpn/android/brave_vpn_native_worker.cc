/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_vpn/android/brave_vpn_native_worker.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "brave/browser/brave_vpn/brave_vpn_service_factory.h"
#include "brave/build/android/jni_headers/BraveVpnNativeWorker_jni.h"
#include "brave/components/brave_vpn/brave_vpn_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

BraveVpnService* GetBraveVpnService() {
  return BraveVpnServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile());
}

}  // namespace

namespace chrome {
namespace android {

BraveVpnNativeWorker::BraveVpnNativeWorker(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj)
    : weak_java_brave_vpn_native_worker_(env, obj), weak_factory_(this) {
  Java_BraveVpnNativeWorker_setNativePtr(env, obj,
                                         reinterpret_cast<intptr_t>(this));
}

BraveVpnNativeWorker::~BraveVpnNativeWorker() {}

void BraveVpnNativeWorker::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delete this;
}

void BraveVpnNativeWorker::GetAllServerRegions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  BraveVpnService* brave_vpn_service = GetBraveVpnService();
  if (brave_vpn_service) {
    brave_vpn_service->GetAllServerRegions(
        base::BindOnce(&BraveVpnNativeWorker::OnGetAllServerRegions,
                       weak_factory_.GetWeakPtr()));
  }
}

void BraveVpnNativeWorker::OnGetAllServerRegions(
    const std::string& server_regions_json,
    bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BraveVpnNativeWorker_onGetAllServerRegions(
      env, weak_java_brave_vpn_native_worker_.get(env),
      base::android::ConvertUTF8ToJavaString(env, server_regions_json),
      success);
}

void BraveVpnNativeWorker::GetTimezonesForRegions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  BraveVpnService* brave_vpn_service = GetBraveVpnService();
  if (brave_vpn_service) {
    brave_vpn_service->GetTimezonesForRegions(
        base::BindOnce(&BraveVpnNativeWorker::OnGetTimezonesForRegions,
                       weak_factory_.GetWeakPtr()));
  }
}

void BraveVpnNativeWorker::OnGetTimezonesForRegions(
    const std::string& timezones_json,
    bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BraveVpnNativeWorker_onGetTimezonesForRegions(
      env, weak_java_brave_vpn_native_worker_.get(env),
      base::android::ConvertUTF8ToJavaString(env, timezones_json), success);
}

void BraveVpnNativeWorker::GetHostnamesForRegion(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& region) {
  BraveVpnService* brave_vpn_service = GetBraveVpnService();
  if (brave_vpn_service) {
    brave_vpn_service->GetHostnamesForRegion(
        base::BindOnce(&BraveVpnNativeWorker::OnGetHostnamesForRegion,
                       weak_factory_.GetWeakPtr()),
        base::android::ConvertJavaStringToUTF8(env, region));
  }
}

void BraveVpnNativeWorker::OnGetHostnamesForRegion(
    const std::string& hostnames_json,
    bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BraveVpnNativeWorker_onGetHostnamesForRegion(
      env, weak_java_brave_vpn_native_worker_.get(env),
      base::android::ConvertUTF8ToJavaString(env, hostnames_json), success);
}

void BraveVpnNativeWorker::GetSubscriberCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& product_type,
    const base::android::JavaParamRef<jstring>& product_id,
    const base::android::JavaParamRef<jstring>& validation_method,
    const base::android::JavaParamRef<jstring>& purchase_token) {
  BraveVpnService* brave_vpn_service = GetBraveVpnService();
  if (brave_vpn_service) {
    brave_vpn_service->GetSubscriberCredential(
        base::BindOnce(&BraveVpnNativeWorker::OnGetSubscriberCredential,
                       weak_factory_.GetWeakPtr()),
        base::android::ConvertJavaStringToUTF8(env, product_type),
        base::android::ConvertJavaStringToUTF8(env, product_id),
        base::android::ConvertJavaStringToUTF8(env, validation_method),
        base::android::ConvertJavaStringToUTF8(env, purchase_token));
  }
}

void BraveVpnNativeWorker::OnGetSubscriberCredential(
    const std::string& subscriber_credential,
    bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BraveVpnNativeWorker_onGetSubscriberCredential(
      env, weak_java_brave_vpn_native_worker_.get(env),
      base::android::ConvertUTF8ToJavaString(env, subscriber_credential),
      success);
}

void BraveVpnNativeWorker::VerifyPurchaseToken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& purchase_token,
    const base::android::JavaParamRef<jstring>& product_id,
    const base::android::JavaParamRef<jstring>& product_type) {
  BraveVpnService* brave_vpn_service = GetBraveVpnService();
  if (brave_vpn_service) {
    brave_vpn_service->VerifyPurchaseToken(
        base::BindOnce(&BraveVpnNativeWorker::OnVerifyPurchaseToken,
                       weak_factory_.GetWeakPtr()),
        base::android::ConvertJavaStringToUTF8(env, purchase_token),
        base::android::ConvertJavaStringToUTF8(env, product_id),
        base::android::ConvertJavaStringToUTF8(env, product_type));
  }
}

void BraveVpnNativeWorker::OnVerifyPurchaseToken(
    const std::string& json_response,
    bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BraveVpnNativeWorker_onVerifyPurchaseToken(
      env, weak_java_brave_vpn_native_worker_.get(env),
      base::android::ConvertUTF8ToJavaString(env, json_response), success);
}

static void JNI_BraveVpnNativeWorker_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  new BraveVpnNativeWorker(env, jcaller);
}

}  // namespace android
}  // namespace chrome
