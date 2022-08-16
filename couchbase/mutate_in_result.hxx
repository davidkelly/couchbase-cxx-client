/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *   Copyright 2020-Present Couchbase, Inc.
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

#pragma once

#include <couchbase/codec/json_transcoder.hxx>
#include <couchbase/mutation_result.hxx>

#include <optional>

namespace couchbase
{

/**
 * Represents result of mutate_in operations.
 *
 * @since 1.0.0
 * @committed
 */
class mutate_in_result : public mutation_result
{
  public:
    /**
     * @since 1.0.0
     * @internal
     */
    struct entry {
        std::string path;
        codec::binary value;
        std::size_t original_index;
    };

    /**
     * @since 1.0.0
     * @internal
     */
    mutate_in_result() = default;

    /**
     * Constructs result for mutate_in_result operation
     *
     * @param cas
     * @param token mutate_in token returned by the server
     * @param entries list of the fields returned by the server
     *
     * @since 1.0.0
     * @committed
     */
    mutate_in_result(couchbase::cas cas, couchbase::mutation_token token, std::vector<entry> entries)
      : mutation_result{ cas, std::move(token) }
      , entries_(std::move(entries))
    {
    }

    /**
     * Decodes field of the document into type.
     *
     * @tparam Document custom type that codec::json_transcoder should use to decode the field
     * @return decoded document content
     *
     * @since 1.0.0
     * @committed
     */
    template<typename Document, std::enable_if_t<!codec::is_transcoder_v<Document>, bool> = true>
    [[nodiscard]] auto content_as(std::size_t index) const -> Document
    {
        for (const entry& e : entries_) {
            if (e.original_index == index) {
                return codec::json_transcoder::template decode<Document>(e.value);
            }
        }
        throw std::system_error(errc::common::invalid_argument, "invalid index for mutate_in result: {}" + std::to_string(index));
    }

    /**
     * Decodes field of the document into type.
     *
     * @tparam Document custom type that codec::json_transcoder should use to decode the field
     * @return decoded document content
     *
     * @since 1.0.0
     * @committed
     */
    template<typename Document, std::enable_if_t<!codec::is_transcoder_v<Document>, bool> = true>
    [[nodiscard]] auto content_as(const std::string& path) const -> Document
    {
        for (const entry& e : entries_) {
            if (e.path == path) {
                return codec::json_transcoder::template decode<Document>(e.value);
            }
        }
        throw std::system_error(errc::common::invalid_argument, "invalid path for mutate_in result: " + path);
    }

  private:
    std::vector<entry> entries_{};
};

} // namespace couchbase