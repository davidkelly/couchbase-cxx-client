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

#include "command_bundle.hxx"
#include "opcode.hxx"
#include "path_flags.hxx"

#include <couchbase/codec/json_transcoder.hxx>
#include <couchbase/subdoc/counter.hxx>

namespace couchbase
{
void
subdoc::counter::encode(core::impl::subdoc::command_bundle& bundle) const
{
    bundle.emplace_back({
      core::impl::subdoc::opcode::counter,
      path_,
      std::move(codec::json_transcoder::encode(delta_).data),
      core::impl::subdoc::build_mutate_in_path_flags(xattr_, create_path_, false),
    });
}
} // namespace couchbase
