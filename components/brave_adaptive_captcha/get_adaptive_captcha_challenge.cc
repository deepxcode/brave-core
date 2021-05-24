/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_adaptive_captcha/get_adaptive_captcha_challenge.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"

namespace {

const char kDevelopment[] = "https://grant.rewards.brave.software";
const char kStaging[] = "https://grant.rewards.bravesoftware.com";
const char kProduction[] = "https://grant.rewards.brave.com";

std::string GetServerUrl(brave_adaptive_captcha::Environment environment,
                         const std::string& path) {
  DCHECK(!path.empty());

  std::string url;
  switch (environment) {
    case brave_adaptive_captcha::Environment::DEVELOPMENT:
      url = kDevelopment;
      break;
    case brave_adaptive_captcha::Environment::STAGING:
      url = kStaging;
      break;
    case brave_adaptive_captcha::Environment::PRODUCTION:
      url = kProduction;
      break;
  }

  return url + path;
}

}  // namespace

namespace brave_adaptive_captcha {

GetAdaptiveCaptchaChallenge::GetAdaptiveCaptchaChallenge(
    UrlLoader* url_loader,
    Environment environment)
    : url_loader_(url_loader), environment_(environment) {
  DCHECK(url_loader);
}

GetAdaptiveCaptchaChallenge::~GetAdaptiveCaptchaChallenge() = default;

std::string GetAdaptiveCaptchaChallenge::GetUrl(const std::string& payment_id) {
  const std::string path =
      base::StringPrintf("/v3/captcha/challenge/%s", payment_id.c_str());

  return GetServerUrl(environment_, path);
}

bool GetAdaptiveCaptchaChallenge::CheckStatusCode(int status_code) {
  if (status_code == net::HTTP_NOT_FOUND) {
    LOG(ERROR) << "No captcha found for given payment id";
    return false;
  }

  if (status_code == net::HTTP_INTERNAL_SERVER_ERROR) {
    LOG(ERROR) << "Failed to retrieve the captcha";
    return false;
  }

  if (status_code != net::HTTP_OK) {
    LOG(ERROR) << "Unexpected HTTP status: " << status_code;
    return false;
  }

  return true;
}

bool GetAdaptiveCaptchaChallenge::ParseBody(const std::string& body,
                                            std::string* captcha_id) {
  DCHECK(captcha_id);

  base::Optional<base::Value> value = base::JSONReader::Read(body);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Invalid JSON";
    return false;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "Invalid JSON";
    return false;
  }

  const std::string* captcha_id_value = dictionary->FindStringKey("captchaID");
  if (!captcha_id_value) {
    LOG(ERROR) << "Missing captcha id";
    return false;
  }

  *captcha_id = *captcha_id_value;

  return true;
}

void GetAdaptiveCaptchaChallenge::Request(
    const std::string& payment_id,
    OnGetAdaptiveCaptchaChallenge callback) {
  auto url_callback =
      base::BindOnce(&GetAdaptiveCaptchaChallenge::OnRequest,
                     base::Unretained(this), std::move(callback));

  UrlLoader::UrlRequest url_request;
  url_request.url = GetUrl(payment_id);

  url_loader_->Load(url_request, std::move(url_callback));
}

void GetAdaptiveCaptchaChallenge::OnRequest(
    OnGetAdaptiveCaptchaChallenge callback,
    const UrlLoader::UrlResponse& url_response) {
  //  ledger::LogUrlResponse(__func__, url_response, true);

  bool result = CheckStatusCode(url_response.status_code);
  if (!result) {
    std::move(callback).Run("");
    return;
  }

  std::string captcha_id;
  result = ParseBody(url_response.body, &captcha_id);
  if (!result) {
    std::move(callback).Run("");
    return;
  }

  std::move(callback).Run(captcha_id);
}

}  // namespace brave_adaptive_captcha
