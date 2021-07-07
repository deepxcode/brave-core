/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <tuple>
#include <utility>

#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "bat/ledger/global_constants.h"
#include "bat/ledger/internal/database/database_mock.h"
#include "bat/ledger/internal/ledger_client_mock.h"
#include "bat/ledger/internal/ledger_impl_mock.h"
#include "bat/ledger/internal/state/state_keys.h"
#include "bat/ledger/internal/uphold/uphold.h"
#include "bat/ledger/ledger.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=UpholdTest.*

using std::placeholders::_1;
using std::placeholders::_2;

using ::testing::_;
using ::testing::AtMost;
using ::testing::Between;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

namespace ledger {
namespace uphold {

class UpholdTest : public Test {
 private:
  base::test::TaskEnvironment scoped_task_environment_;

 protected:
  std::unique_ptr<MockLedgerClient> mock_ledger_client_;
  std::unique_ptr<MockLedgerImpl> mock_ledger_impl_;
  std::unique_ptr<database::MockDatabase> mock_database_;
  std::unique_ptr<Uphold> uphold_;

  UpholdTest()
      : mock_ledger_client_{std::make_unique<MockLedgerClient>()},
        mock_ledger_impl_{
            std::make_unique<MockLedgerImpl>(mock_ledger_client_.get())},
        mock_database_{
            std::make_unique<database::MockDatabase>(mock_ledger_impl_.get())},
        uphold_{std::make_unique<Uphold>(mock_ledger_impl_.get())} {}
};

TEST_F(UpholdTest, FetchBalanceConnectedWallet) {
  const std::string wallet = R"({
      "token":"token",
      "address":"address"
      "status":1
    })";
  ON_CALL(*mock_ledger_client_, GetEncryptedStringState(state::kWalletUphold))
      .WillByDefault(Return(wallet));
  EXPECT_CALL(*mock_ledger_client_, LoadURL(_, _)).Times(0);

  FetchBalanceCallback callback = std::bind(
      [&](type::Result result, double balance) {
        ASSERT_EQ(result, type::Result::LEDGER_OK);
        ASSERT_EQ(balance, 0.0);
      },
      _1, _2);

  uphold_->FetchBalance(callback);
}

absl::optional<type::WalletStatus> GetStatusFromJSON(
    const std::string& uphold_wallet) {
  auto value = base::JSONReader::Read(uphold_wallet);
  if (value && value->is_dict()) {
    base::DictionaryValue* dictionary = nullptr;
    if (value->GetAsDictionary(&dictionary)) {
      if (auto status = dictionary->FindIntKey("status")) {
        return static_cast<type::WalletStatus>(*status);
      }
    }
  }

  return {};
}

template <typename ParamType>
std::string NameSuffixGenerator(const TestParamInfo<ParamType>& info) {
  return std::get<0>(info.param);
}

// clang-format off
using AuthorizeParamType = std::tuple<
    std::string,                               // test name suffix
    std::string,                               // Uphold wallet (1)
    bool,                                      // SetWallet() returns (1)
    base::flat_map<std::string, std::string>,  // input args
    type::UrlResponse,                         // Uphold response
    std::string,                               // Uphold wallet (2)
    bool,                                      // SetWallet() returns (2)
    type::Result,                              // expected result
    base::flat_map<std::string, std::string>,  // expected args
    absl::optional<type::WalletStatus>         // expected status
>;

struct Authorize : UpholdTest,
                   WithParamInterface<AuthorizeParamType> {};

INSTANTIATE_TEST_SUITE_P(
  UpholdTest,
  Authorize,
  Values(
    AuthorizeParamType{  // Uphold wallet is null!
      "00_uphold_wallet_is_null",
      {},
      false,
      {},
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {},
      {}
    },
    AuthorizeParamType{  // Attempting to re-authorize in VERIFIED status!
      "01_attempting_to_re_authorize_in_verified_status",
      R"({ "status": 2 })",
      false,
      {},
      {},
      R"({ "status": 2 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::VERIFIED
    },
    AuthorizeParamType{  // Unable to set the Uphold wallet!
      "02_unable_to_set_the_uphold_wallet",
      R"({ "status": 0 })",
      false,
      {},
      {},
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    // NOLINTNEXTLINE
    AuthorizeParamType{  // Uphold returned with an error - the user is not KYC'd
      "03_uphold_returned_with_user_does_not_meet_minimum_requirements",
      R"({ "status": 0 })",
      true,
      { { "error_description", "User does not meet minimum requirements" } },
      {},
      R"({ "status": 0 })",
      false,
      type::Result::NOT_FOUND,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    // NOLINTNEXTLINE
    AuthorizeParamType{  // Uphold returned with an error - theoretically not possible
      "04_uphold_returned_with_an_error",
      R"({ "status": 0 })",
      true,
      { { "error_description", "some other reason" } },
      {},
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // Arguments are empty!
      "05_arguments_are_empty",
      R"({ "status": 0 })",
      true,
      {},
      {},
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // code is empty!
      "06_code_is_empty",
      R"({ "status": 0 })",
      true,
      { { "code", "" } },
      {},
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // state is empty!
      "07_state_is_empty",
      R"({ "status": 0 })",
      true,
      { { "code", "code" }, { "state", "" } },
      {},
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // One-time string mismatch!
      "08_one_time_string_mismatch",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "mismatch" } },
      {},
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // Uphold wallet is null!
      "09_uphold_wallet_is_null",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {},
      {}
    },
    AuthorizeParamType{  // Attempting to re-authorize in VERIFIED status!
      "10_attempting_to_re_authorize_in_verified_status",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      {},
      R"({ "status": 2 })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::VERIFIED
    },
    AuthorizeParamType{  // Couldn't exchange code for the access token!
      "11_couldn_t_exchange_code_for_the_access_token",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      type::UrlResponse{
        {},
        {},
        net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR,
        {},
        {}
      },
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // Access token is empty!
      "12_access_token_is_empty",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      type::UrlResponse{
        {},
        {},
        net::HttpStatusCode::HTTP_OK,
        R"({ "access_token": "" })",
        {}
      },
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // Unable to set the Uphold wallet!
      "13_unable_to_set_the_uphold_wallet",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      type::UrlResponse{
        {},
        {},
        net::HttpStatusCode::HTTP_OK,
        R"({ "access_token": "access_token" })",
        {}
      },
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      false,
      type::Result::LEDGER_ERROR,
      {},
      type::WalletStatus::NOT_CONNECTED
    },
    AuthorizeParamType{  // happy path
      "14_happy_path",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      type::UrlResponse{
        {},
        {},
        net::HttpStatusCode::HTTP_OK,
        R"({ "access_token": "access_token" })",
        {}
      },
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      type::Result::LEDGER_OK,
      {},
      type::WalletStatus::PENDING
    }),
  NameSuffixGenerator<AuthorizeParamType>
);
// clang-format on

TEST_P(Authorize, Paths) {
  const auto& params = GetParam();
  const auto& uphold_wallet_1 = std::get<1>(params);
  const auto set_wallet_1 = std::get<2>(params);
  const auto& input_args = std::get<3>(params);
  const auto& uphold_response = std::get<4>(params);
  const auto& uphold_wallet_2 = std::get<5>(params);
  const auto set_wallet_2 = std::get<6>(params);
  const auto expected_result = std::get<7>(params);
  const auto& expected_args = std::get<8>(params);
  const auto expected_status = std::get<9>(params);

  std::string uphold_wallet{};

  EXPECT_CALL(*mock_ledger_client_,
              GetEncryptedStringState(state::kWalletUphold))
      .Times(Between(1, 2))
      .WillOnce([&] { return uphold_wallet = uphold_wallet_1; })
      .WillOnce([&] { return uphold_wallet = uphold_wallet_2; });

  EXPECT_CALL(*mock_ledger_client_,
              SetEncryptedStringState(state::kWalletUphold, _))
      .Times(AtMost(2))
      .WillOnce(Invoke([&](const std::string&, const std::string& value) {
        if (set_wallet_1) {
          uphold_wallet = value;
        }
        return set_wallet_1;
      }))
      .WillOnce(Invoke([&](const std::string&, const std::string& value) {
        if (set_wallet_2) {
          uphold_wallet = value;
        }
        return set_wallet_2;
      }));

  ON_CALL(*mock_ledger_client_, LoadURL(_, _))
      .WillByDefault(
          Invoke([&](type::UrlRequestPtr, client::LoadURLCallback callback) {
            callback(uphold_response);
          }));

  ON_CALL(*mock_ledger_impl_, database())
      .WillByDefault(Return(mock_database_.get()));

  uphold_->WalletAuthorization(
      input_args,
      [&](type::Result result, base::flat_map<std::string, std::string> args) {
        ASSERT_EQ(result, expected_result);
        ASSERT_EQ(args, expected_args);

        const auto status = GetStatusFromJSON(uphold_wallet);
        if (status && expected_status) {
          ASSERT_EQ(*status, *expected_status);
        } else {
          ASSERT_TRUE(!status && !expected_status);
        }
      });
}

// clang-format off
using GenerateParamType = std::tuple<
    std::string,                        // test name suffix
    std::string,                        // Uphold wallet (1)
    bool,                               // SetWallet() returns (1)
    bool,                               // SetWallet() returns (2)
    type::Result,                       // expected result
    absl::optional<type::WalletStatus>  // expected status
>;

struct Generate : UpholdTest,
                  WithParamInterface<GenerateParamType> {};

INSTANTIATE_TEST_SUITE_P(
  UpholdTest,
  Generate,
  Values(
    GenerateParamType{  // Unable to set the Uphold wallet!
      "00_unable_to_set_the_uphold_wallet",
      {},
      false,
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    GenerateParamType{  // Unable to set the Uphold wallet!
      "01_unable_to_set_the_uphold_wallet",
      {},
      true,
      false,
      type::Result::LEDGER_ERROR,
      type::WalletStatus::NOT_CONNECTED
    },
    GenerateParamType{  // happy path
      "02_happy_path",
      {},
      true,
      true,
      type::Result::LEDGER_OK,
      type::WalletStatus::NOT_CONNECTED
    },
    GenerateParamType{  // Unable to set the Uphold wallet!
      "03_unable_to_set_the_uphold_wallet",
      R"({ "status": 0 })",
      false,
      false,
      type::Result::LEDGER_ERROR,
      type::WalletStatus::NOT_CONNECTED
    },
    GenerateParamType{  // happy path
      "04_happy_path",
      R"({ "status": 0 })",
      true,
      false,
      type::Result::LEDGER_OK,
      type::WalletStatus::NOT_CONNECTED
    }),
  NameSuffixGenerator<GenerateParamType>
);
// clang-format on

TEST_P(Generate, Paths) {
  const auto& params = GetParam();
  const auto& uphold_wallet_1 = std::get<1>(params);
  const auto set_wallet_1 = std::get<2>(params);
  const auto set_wallet_2 = std::get<3>(params);
  const auto expected_result = std::get<4>(params);
  const auto expected_status = std::get<5>(params);

  std::string uphold_wallet{};

  EXPECT_CALL(*mock_ledger_client_,
              GetEncryptedStringState(state::kWalletUphold))
      .Times(Exactly(1))
      .WillOnce([&] { return uphold_wallet = uphold_wallet_1; });

  EXPECT_CALL(*mock_ledger_client_,
              SetEncryptedStringState(state::kWalletUphold, _))
      .Times(AtMost(2))
      .WillOnce(Invoke([&](const std::string&, const std::string& value) {
        if (set_wallet_1) {
          uphold_wallet = value;
        }
        return set_wallet_1;
      }))
      .WillOnce(Invoke([&](const std::string&, const std::string& value) {
        if (set_wallet_2) {
          uphold_wallet = value;
        }
        return set_wallet_2;
      }));

  ON_CALL(*mock_ledger_impl_, database())
      .WillByDefault(Return(mock_database_.get()));

  uphold_->GenerateWallet([&](type::Result result) {
    ASSERT_EQ(result, expected_result);

    const auto status = GetStatusFromJSON(uphold_wallet);
    if (status && expected_status) {
      ASSERT_EQ(*status, *expected_status);
    } else {
      ASSERT_TRUE(!status && !expected_status);
    }
  });
}

}  // namespace uphold
}  // namespace ledger
