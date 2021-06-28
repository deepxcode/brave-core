/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "bat/ledger/internal/endpoint/promotion/get_wallet/get_wallet.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "bat/ledger/internal/endpoint/promotion/promotions_util.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "net/http/http_status_code.h"

using std::placeholders::_1;

namespace ledger {
namespace endpoint {
namespace promotion {

GetWallet::GetWallet(LedgerImpl* ledger) : ledger_{ledger} {
  DCHECK(ledger_);
}

GetWallet::~GetWallet() = default;

void GetWallet::Request(GetWalletCallback callback) const {
  auto request = type::UrlRequest::New();
  request->url = GetUrl();
  ledger_->LoadURL(std::move(request),
                   std::bind(&GetWallet::OnRequest, this, _1, callback));
}

std::string GetWallet::GetUrl() const {
  const auto rewards_wallet = ledger_->wallet()->GetWallet();
  if (!rewards_wallet) {
    BLOG(0, "Rewards wallet is null!");
    return {};
  }

  return GetServerUrl(
      base::StringPrintf("/v3/wallet/%s", rewards_wallet->payment_id.c_str()));
}

void GetWallet::OnRequest(const type::UrlResponse& response,
                          GetWalletCallback callback) const {
  ledger::LogUrlResponse(__func__, response);

  type::Result result = CheckStatusCode(response.status_code);
  if (result != type::Result::LEDGER_OK) {
    return callback(result, false);
  }

  bool linked{};
  result = ParseBody(response.body, linked);
  callback(result, linked);
}

type::Result GetWallet::CheckStatusCode(int status_code) const {
  if (status_code == net::HTTP_BAD_REQUEST) {
    BLOG(0, "Invalid payment id");
    return type::Result::LEDGER_ERROR;
  }

  if (status_code == net::HTTP_NOT_FOUND) {
    BLOG(0, "Unrecognized payment id");
    return type::Result::LEDGER_ERROR;
  }

  if (status_code != net::HTTP_OK) {
    BLOG(0, "Unexpected HTTP status: " << status_code);
    return type::Result::LEDGER_ERROR;
  }

  return type::Result::LEDGER_OK;
}

type::Result GetWallet::ParseBody(const std::string& body, bool& linked) const {
  base::Optional<base::Value> value = base::JSONReader::Read(body);
  if (!value || !value->is_dict()) {
    BLOG(0, "Invalid JSON");
    return type::Result::LEDGER_ERROR;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    BLOG(0, "Invalid JSON");
    return type::Result::LEDGER_ERROR;
  }

  const std::string* linking_id =
      dictionary->FindStringPath("depositAccountProvider.linkingId");

  if (!linking_id) {
    return type::Result::LEDGER_ERROR;
  }

  linked = !linking_id->empty();

  return type::Result::LEDGER_OK;
}

}  // namespace promotion
}  // namespace endpoint
}  // namespace ledger
