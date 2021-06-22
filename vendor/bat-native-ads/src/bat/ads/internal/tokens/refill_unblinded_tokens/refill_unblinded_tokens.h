/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_REFILL_UNBLINDED_TOKENS_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_REFILL_UNBLINDED_TOKENS_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "bat/ads/internal/account/wallet/wallet_info.h"
#include "bat/ads/internal/backoff_timer.h"
#include "bat/ads/internal/privacy/tokens/token_generator_interface.h"
#include "bat/ads/internal/tokens/get_issuers/get_issuers.h"
#include "bat/ads/internal/tokens/refill_unblinded_tokens/refill_unblinded_tokens_delegate.h"
#include "bat/ads/mojom.h"
#include "bat/ads/result.h"
#include "wrapper.hpp"

namespace ads {

using challenge_bypass_ristretto::BlindedToken;
using challenge_bypass_ristretto::Token;

class RefillUnblindedTokens {
 public:
  explicit RefillUnblindedTokens(
      privacy::TokenGeneratorInterface* token_generator);

  ~RefillUnblindedTokens();

  void set_delegate(RefillUnblindedTokensDelegate* delegate);

  void MaybeRefill(const WalletInfo& wallet);

 private:
  WalletInfo wallet_;
  std::string nonce_;

  std::vector<Token> tokens_;
  std::vector<BlindedToken> blinded_tokens_;

  void Refill();

  void RequestIssuers();
  void OnRequestIssuers();

  void RequestSignedTokens();
  void OnRequestSignedTokens(const UrlResponse& url_response);

  void GetSignedTokens();
  void OnGetSignedTokens(const UrlResponse& url_response);

  void OnDidRefillUnblindedTokens();

  void OnFailedToRefillUnblindedTokens(const bool should_retry);

  BackoffTimer retry_timer_;
  void Retry();
  void OnRetry();

  bool ShouldRefillUnblindedTokens() const;

  int CalculateAmountOfTokensToRefill() const;

  bool is_processing_ = false;

  std::unique_ptr<GetIssuers> get_issuers_;

  privacy::TokenGeneratorInterface* token_generator_;  // NOT OWNED

  RefillUnblindedTokensDelegate* delegate_ = nullptr;
};

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_REFILL_UNBLINDED_TOKENS_H_
