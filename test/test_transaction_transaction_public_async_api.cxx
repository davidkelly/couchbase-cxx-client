/*
 *     Copyright 2022 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "test_helper.hxx"
#include "utils/transactions_env.h"

static const tao::json::value async_content{ { "some_number", 0 } };

TEST_CASE("can async get", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [id, coll](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [id](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK_FALSE(res->ctx().ec());
              CHECK(res->key() == id.key());
              CHECK(res->bucket() == id.bucket());
              CHECK(res->scope() == id.scope());
              CHECK(res->content<tao::json::value>() == async_content);
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.ctx.ec());
          CHECK_FALSE(res.transaction_id.empty());
          CHECK_FALSE(res.unstaging_complete);
          barrier->set_value();
      });
    f.get();
}

TEST_CASE("can get fail as expected", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [id, coll](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [id](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK(res->ctx().ec());
              CHECK(res->ctx().ec() == couchbase::errc::transaction_op::document_not_found_exception);
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.ctx.ec());
          CHECK_FALSE(res.transaction_id.empty());
          CHECK_FALSE(res.unstaging_complete);
          barrier->set_value();
      });
    f.get();
}
TEST_CASE("can async remove", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [coll, id](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [&ctx](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK_FALSE(res->ctx().ec());
              ctx.remove(res, [](couchbase::transaction_op_error_context err) { CHECK_FALSE(err.ec()); });
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.transaction_id.empty());
          CHECK(res.unstaging_complete);
          CHECK_FALSE(res.ctx.ec());
          barrier->set_value();
      });
    f.get();
}

TEST_CASE("async remove with bad cas fails as expected", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [coll, id](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [&ctx](couchbase::transactions::transaction_get_result_ptr res) {
              std::reinterpret_pointer_cast<couchbase::core::transactions::transaction_get_result>(res)->cas(100);
              ctx.remove(res, [](couchbase::transaction_op_error_context err) { CHECK(err.ec()); });
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.transaction_id.empty());
          CHECK_FALSE(res.unstaging_complete);
          CHECK(res.ctx.ec() == couchbase::errc::transaction::expired);
          barrier->set_value();
      });
    f.get();
}
TEST_CASE("can async insert", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [id, coll](couchbase::transactions::async_attempt_context& ctx) {
          ctx.insert(coll, id.key(), async_content, [coll, id](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK_FALSE(res->ctx().ec());
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.transaction_id.empty());
          CHECK(res.unstaging_complete);
          CHECK_FALSE(res.ctx.ec());
          barrier->set_value();
      });
    f.get();
}

TEST_CASE("async insert fails when doc already exists", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    c.transactions()->run(
      [id, coll](couchbase::transactions::async_attempt_context& ctx) {
          ctx.insert(coll, id.key(), async_content, [coll, id](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK(res->ctx().ec() == couchbase::errc::transaction_op::document_exists_exception);
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK_FALSE(res.transaction_id.empty());
          CHECK_FALSE(res.unstaging_complete);
          CHECK(res.ctx.ec() == couchbase::errc::transaction::failed);
          CHECK(res.ctx.cause() == couchbase::errc::transaction_op::document_exists_exception);
          barrier->set_value();
      });
    f.get();
}

TEST_CASE("can async replace", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    tao::json::value new_content = { { "Iam", "new content" } };
    c.transactions()->run(
      [id, coll, new_content](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [new_content, &ctx](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK_FALSE(res->ctx().ec());
              ctx.replace(res, new_content, [](couchbase::transactions::transaction_get_result_ptr replace_res) {
                  CHECK_FALSE(replace_res->ctx().ec());
              });
          });
      },
      [barrier](couchbase::transactions::transaction_result tx_result) {
          CHECK_FALSE(tx_result.transaction_id.empty());
          CHECK(tx_result.unstaging_complete);
          CHECK_FALSE(tx_result.ctx.ec());
          barrier->set_value();
      });
    f.get();
}
TEST_CASE("async replace fails as expected with bad cas", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    tao::json::value new_content = { { "Iam", "new content" } };
    c.transactions()->run(
      [id, coll, new_content](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [new_content, &ctx](couchbase::transactions::transaction_get_result_ptr res) {
              std::reinterpret_pointer_cast<couchbase::core::transactions::transaction_get_result>(res)->cas(100);
              ctx.replace(
                res, new_content, [](couchbase::transactions::transaction_get_result_ptr replace_res) { CHECK(replace_res->ctx().ec()); });
          });
      },
      [barrier](couchbase::transactions::transaction_result tx_result) {
          CHECK_FALSE(tx_result.transaction_id.empty());
          CHECK_FALSE(tx_result.unstaging_complete);
          CHECK(tx_result.ctx.ec() == couchbase::errc::transaction::expired);
          barrier->set_value();
      });
    f.get();
}

TEST_CASE("uncaught exception will rollback", "[transactions]")
{
    auto id = TransactionsTestEnvironment::get_document_id();
    REQUIRE(TransactionsTestEnvironment::upsert_doc(id, async_content));
    auto core_cluster = TransactionsTestEnvironment::get_cluster();
    couchbase::cluster c(core_cluster);
    auto coll = std::make_shared<couchbase::collection>(c.bucket("default").default_collection());
    auto barrier = std::make_shared<std::promise<void>>();
    auto f = barrier->get_future();
    tao::json::value new_content = { { "Iam", "new content" } };
    c.transactions()->run(
      [id, coll, new_content](couchbase::transactions::async_attempt_context& ctx) {
          ctx.get(coll, id.key(), [new_content, &ctx](couchbase::transactions::transaction_get_result_ptr res) {
              CHECK_FALSE(res->ctx().ec());
              ctx.replace(res, new_content, [](couchbase::transactions::transaction_get_result_ptr res) {
                  CHECK_FALSE(res->ctx().ec());
                  throw std::runtime_error("I wanna rollback");
              });
          });
      },
      [barrier](couchbase::transactions::transaction_result res) {
          CHECK(res.ctx.ec() == couchbase::errc::transaction::failed);
          CHECK_FALSE(res.unstaging_complete);
          CHECK_FALSE(res.transaction_id.empty());
          barrier->set_value();
      });
    f.get();
}
