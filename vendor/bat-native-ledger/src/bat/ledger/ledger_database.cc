/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/ledger_database.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "bat/ledger/internal/logging/logging.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace ledger {

namespace {

void HandleBinding(sql::Statement* statement,
                   const mojom::DBCommandBinding& binding) {
  DCHECK(statement);

  switch (binding.value->which()) {
    case mojom::DBValue::Tag::STRING_VALUE:
      statement->BindString(binding.index, binding.value->get_string_value());
      break;
    case mojom::DBValue::Tag::INT_VALUE:
      statement->BindInt(binding.index, binding.value->get_int_value());
      break;
    case mojom::DBValue::Tag::INT64_VALUE:
      statement->BindInt64(binding.index, binding.value->get_int64_value());
      break;
    case mojom::DBValue::Tag::DOUBLE_VALUE:
      statement->BindDouble(binding.index, binding.value->get_double_value());
      break;
    case mojom::DBValue::Tag::BOOL_VALUE:
      statement->BindBool(binding.index, binding.value->get_bool_value());
      break;
    case mojom::DBValue::Tag::NULL_VALUE:
      statement->BindNull(binding.index);
      break;
    default:
      NOTREACHED();
  }
}

mojom::DBRecordPtr CreateRecord(
    sql::Statement* statement,
    const std::vector<mojom::DBCommand::RecordBindingType>& bindings) {
  DCHECK(statement);

  auto record = mojom::DBRecord::New();
  int column = 0;

  for (const auto& binding : bindings) {
    auto value = mojom::DBValue::New();
    switch (binding) {
      case mojom::DBCommand::RecordBindingType::STRING_TYPE: {
        value->set_string_value(statement->ColumnString(column));
        break;
      }
      case mojom::DBCommand::RecordBindingType::INT_TYPE: {
        value->set_int_value(statement->ColumnInt(column));
        break;
      }
      case mojom::DBCommand::RecordBindingType::INT64_TYPE: {
        value->set_int64_value(statement->ColumnInt64(column));
        break;
      }
      case mojom::DBCommand::RecordBindingType::DOUBLE_TYPE: {
        value->set_double_value(statement->ColumnDouble(column));
        break;
      }
      case mojom::DBCommand::RecordBindingType::BOOL_TYPE: {
        value->set_bool_value(statement->ColumnBool(column));
        break;
      }
      default: {
        NOTREACHED();
      }
    }
    record->fields.push_back(std::move(value));
    column++;
  }

  return record;
}

}  // namespace

LedgerDatabase::LedgerDatabase(const base::FilePath& path) : db_path_(path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LedgerDatabase::~LedgerDatabase() = default;

mojom::DBCommandResponsePtr LedgerDatabase::RunTransaction(
    mojom::DBTransactionPtr transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction);

  auto command_response = mojom::DBCommandResponse::New();

  if (!db_.is_open() && !db_.Open(db_path_)) {
    command_response->status =
        mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
    return command_response;
  }

  // Close command must always be sent as single command in transaction
  if (transaction->commands.size() == 1 &&
      transaction->commands[0]->type == mojom::DBCommand::Type::CLOSE) {
    CloseDatabase();
    command_response->status = mojom::DBCommandResponse::Status::RESPONSE_OK;
    return command_response;
  }

  sql::Transaction committer(&db_);
  if (!committer.Begin()) {
    command_response->status =
        mojom::DBCommandResponse::Status::TRANSACTION_ERROR;
    return command_response;
  }

  bool vacuum_requested = false;

  for (auto const& command : transaction->commands) {
    mojom::DBCommandResponse::Status status;

    BLOG(8, "Query: " << command->command);

    switch (command->type) {
      case mojom::DBCommand::Type::INITIALIZE: {
        status =
            Initialize(transaction->version, transaction->compatible_version,
                       command_response.get());
        break;
      }
      case mojom::DBCommand::Type::READ: {
        status = Read(command.get(), command_response.get());
        break;
      }
      case mojom::DBCommand::Type::EXECUTE: {
        status = Execute(command.get());
        break;
      }
      case mojom::DBCommand::Type::RUN: {
        status = Run(command.get());
        break;
      }
      case mojom::DBCommand::Type::MIGRATE: {
        status = Migrate(transaction->version, transaction->compatible_version);
        break;
      }
      case mojom::DBCommand::Type::VACUUM: {
        vacuum_requested = true;
        status = mojom::DBCommandResponse::Status::RESPONSE_OK;
        break;
      }
      case mojom::DBCommand::Type::CLOSE: {
        NOTREACHED();
        status = mojom::DBCommandResponse::Status::COMMAND_ERROR;
        break;
      }
    }

    if (status != mojom::DBCommandResponse::Status::RESPONSE_OK) {
      committer.Rollback();
      command_response->status = status;
      return command_response;
    }
  }

  if (!committer.Commit()) {
    command_response->status =
        mojom::DBCommandResponse::Status::TRANSACTION_ERROR;
    return command_response;
  }

  if (vacuum_requested) {
    BLOG(8, "Performing database vacuum");
    if (!db_.Execute("VACUUM")) {
      // If vacuum was not successful, log an error but do not
      // prevent forward progress.
      BLOG(0, "Error executing VACUUM: " << db_.GetErrorMessage());
    }
  }

  return command_response;
}

bool LedgerDatabase::DeleteDatabaseFile() {
  CloseDatabase();
  return base::DeleteFile(db_path_) &&
         base::DeleteFile(db_path_.AppendASCII("-journal"));
}

void LedgerDatabase::CloseDatabase() {
  db_.Close();
  meta_table_.Reset();
  initialized_ = false;
}

mojom::DBCommandResponse::Status LedgerDatabase::Initialize(
    const int32_t version,
    const int32_t compatible_version,
    mojom::DBCommandResponse* command_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(command_response);

  int table_version = 0;
  if (!initialized_) {
    bool table_exists = false;
    if (meta_table_.DoesTableExist(&db_)) {
      table_exists = true;
    }

    if (!meta_table_.Init(&db_, version, compatible_version)) {
      return mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
    }

    if (table_exists) {
      table_version = meta_table_.GetVersionNumber();
    }

    initialized_ = true;
  } else {
    table_version = meta_table_.GetVersionNumber();
  }

  if (!memory_pressure_listener_) {
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        FROM_HERE, base::BindRepeating(&LedgerDatabase::OnMemoryPressure,
                                       base::Unretained(this))));
  }

  auto value = mojom::DBValue::New();
  value->set_int_value(table_version);
  auto result = mojom::DBCommandResult::New();
  result->set_value(std::move(value));
  command_response->result = std::move(result);

  return mojom::DBCommandResponse::Status::RESPONSE_OK;
}

mojom::DBCommandResponse::Status LedgerDatabase::Execute(
    mojom::DBCommand* command) {
  DCHECK(command);

  if (!initialized_) {
    return mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
  }

  bool result = db_.Execute(command->command.c_str());

  if (!result) {
    BLOG(0, "DB Execute error: " << db_.GetErrorMessage());
    return mojom::DBCommandResponse::Status::COMMAND_ERROR;
  }

  return mojom::DBCommandResponse::Status::RESPONSE_OK;
}

mojom::DBCommandResponse::Status LedgerDatabase::Run(
    mojom::DBCommand* command) {
  DCHECK(command);

  if (!initialized_) {
    return mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
  }

  sql::Statement statement(db_.GetUniqueStatement(command->command.c_str()));

  for (auto const& binding : command->bindings) {
    HandleBinding(&statement, *binding.get());
  }

  if (!statement.Run()) {
    BLOG(0, "DB Run error: " << db_.GetErrorMessage() << " ("
                             << db_.GetErrorCode() << ")");
    return mojom::DBCommandResponse::Status::COMMAND_ERROR;
  }

  return mojom::DBCommandResponse::Status::RESPONSE_OK;
}

mojom::DBCommandResponse::Status LedgerDatabase::Read(
    mojom::DBCommand* command,
    mojom::DBCommandResponse* command_response) {
  DCHECK(command);
  DCHECK(command_response);

  if (!initialized_) {
    return mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
  }

  sql::Statement statement(db_.GetUniqueStatement(command->command.c_str()));

  for (auto const& binding : command->bindings) {
    HandleBinding(&statement, *binding.get());
  }

  auto result = mojom::DBCommandResult::New();
  result->set_records(std::vector<mojom::DBRecordPtr>());
  command_response->result = std::move(result);
  while (statement.Step()) {
    command_response->result->get_records().push_back(
        CreateRecord(&statement, command->record_bindings));
  }

  return mojom::DBCommandResponse::Status::RESPONSE_OK;
}

mojom::DBCommandResponse::Status LedgerDatabase::Migrate(
    const int32_t version,
    const int32_t compatible_version) {
  if (!initialized_) {
    return mojom::DBCommandResponse::Status::INITIALIZATION_ERROR;
  }

  meta_table_.SetVersionNumber(version);
  meta_table_.SetCompatibleVersionNumber(compatible_version);

  return mojom::DBCommandResponse::Status::RESPONSE_OK;
}

void LedgerDatabase::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.TrimMemory();
}

}  // namespace ledger
