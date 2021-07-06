/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utility>

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

// clang-format off
using ParamType = std::tuple<
    std::string,                               // test name suffix
    std::string,                               // Uphold wallet (1)
    bool,                                      // SetWallet() returns (1)
    base::flat_map<std::string, std::string>,  // input args
    type::UrlResponse,                         // Uphold response
    std::string,                               // Uphold wallet (2)
    bool,                                      // SetWallet() returns (2)
    type::Result,                              // expected result
    base::flat_map<std::string, std::string>   // expected args
>;

struct StateMachine : UpholdTest,
                      WithParamInterface<ParamType> {
  static std::string NameSuffixGenerator(
      const TestParamInfo<StateMachine::ParamType>& info) {
    return std::get<0>(info.param);
  }
};

INSTANTIATE_TEST_SUITE_P(
  UpholdTest,
  StateMachine,
  Values(
    ParamType{  // Uphold wallet is null!
      "00_uphold_wallet_is_null",
      {},
      false,
      {},
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Attempting to re-authorize in VERIFIED status!
      "01_attempting_to_re_authorize_in_verified_status",
      R"({ "status": 2 })",
      false,
      {},
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Unable to set the Uphold wallet!
      "02_unable_to_set_the_uphold_wallet",
      R"({ "status": 0 })",
      false,
      {},
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Uphold returned with an error - the user is not KYC'd
      "03_uphold_returned_with_user_does_not_meet_minimum_requirements",
      R"({ "status": 0 })",
      true,
      { { "error_description", "User does not meet minimum requirements" } },
      {},
      {},
      false,
      type::Result::NOT_FOUND,
      {}
    },
    ParamType{  // Uphold returned with an error - theoretically not possible
      "04_uphold_returned_with_an_error",
      R"({ "status": 0 })",
      true,
      { { "error_description", "some other reason" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Arguments are empty!
      "05_arguments_are_empty",
      R"({ "status": 0 })",
      true,
      {},
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // code is empty!
      "06_code_is_empty",
      R"({ "status": 0 })",
      true,
      { { "code", "" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // state is empty!
      "07_state_is_empty",
      R"({ "status": 0 })",
      true,
      { { "code", "code" }, { "state", "" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // One-time string mismatch!
      "08_one_time_string_mismatch",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "mismatch" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Uphold wallet is null!
      "09_uphold_wallet_is_null",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      {},
      {},
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Attempting to re-authorize in VERIFIED status!
      "10_attempting_to_re_authorize_in_verified_status",
      R"({ "status": 0, "one_time_string": "one_time_string" })",
      true,
      { { "code", "code" }, { "state", "one_time_string" } },
      {},
      R"({ "status": 2 })",
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Couldn't exchange code for the access token!
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
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Access token is empty!
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
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // Unable to set the Uphold wallet!
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
      R"({ "status": 0 })",
      false,
      type::Result::LEDGER_ERROR,
      {}
    },
    ParamType{  // success
      "14_success",
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
      R"({ "status": 0 })",
      true,
      type::Result::LEDGER_OK,
      {}
    }
  ),
  StateMachine::NameSuffixGenerator
);

TEST_P(StateMachine, AuthorizationFlow) {
  const auto& params = GetParam();
  const auto& uphold_wallet_1 = std::get<1>(params);
  const auto set_wallet_1 = std::get<2>(params);
  const auto& input_args = std::get<3>(params);
  const auto& uphold_response = std::get<4>(params);
  const auto& uphold_wallet_2 = std::get<5>(params);
  const auto set_wallet_2 = std::get<6>(params);
  const auto expected_result = std::get<7>(params);
  const auto& expected_args = std::get<8>(params);

  EXPECT_CALL(*mock_ledger_client_,
              GetEncryptedStringState(state::kWalletUphold))
      .Times(Between(1, 2))
      .WillOnce(Return(uphold_wallet_1))
      .WillOnce(Return(uphold_wallet_2));

  EXPECT_CALL(*mock_ledger_client_,
          SetEncryptedStringState(state::kWalletUphold, _))
      .Times(AtMost(2))
      .WillOnce(Return(set_wallet_1))
      .WillOnce(Return(set_wallet_2));

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
      });
}

}  // namespace uphold
}  // namespace ledger
