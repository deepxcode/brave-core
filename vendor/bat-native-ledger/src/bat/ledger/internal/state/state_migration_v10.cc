/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/state/state_migration_v10.h"

namespace ledger {

namespace state {

namespace {

struct WalletParseResult {
  std::string payment_id;
  std::string recovery_seed;
};

base::Optional<WalletParseResult> ParseWalletJSON(const std::string& data) {
  base::Optional<base::Value> root = base::JSONReader::Read(data);
  if (!root || !root->is_dict())
    return {};

  auto* payment_id = root->FindStringKey("payment_id");
  if (!payment_id)
    return {};

  auto* seed = root->FindStringKey("recovery_seed");
  if (!seed)
    return {};

  std::string decoded_seed;
  if (!base::Base64Decode(*seed, &decoded_seed))
    return {};

  return WalletParseResult{
      payment_id = *payment_id,
      recovery_seed = std::move(decoded_seed)};
}

}  // namespace

StateMigrationV10::StateMigrationV10(LedgerImpl* ledger) : ledger_(ledger) {}

StateMigrationV10::~StateMigrationV10() = default;

void StateMigrationV10::Migrate(ledger::ResultCallback callback) {
  std::string pref_data =
      ledger_->ledger_client()->GetStringState(kWalletBrave);

  std::string encrypted;
  if (!base::Base64Decode(pref_data, &encrypted)) {
    // TODO(zenparsing): The data found in prefs is not base64
  }

  auto decrypted = ledger_->ledger_client()->DecryptString(encrypted);
  if (!decrypted) {
    // TODO(zenparsing): Unable to decrypt this thing
  }

  auto result = ParseWalletJSON(decrypted);
  if (!result) {
    // TODO(zenparsing): Unable to JSON parse this thing
  }

  auto on_saved = [](ledger::ResultCallback callback) {
    callback(type::Result::LEDGER_OK);
  };

  ledger_->context()->Get<RewardsWalletStore>()->SaveWalletInfo(
      result->payment_id, result->recovery_seed).Then(base::BindOnce(on_saved));

  callback(type::Result::LEDGER_OK);
}

}  // namespace state
}  // namespace ledger
