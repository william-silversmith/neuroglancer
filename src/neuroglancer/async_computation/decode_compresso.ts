/**
 * @license
 * Copyright 2019 Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {TypedArray} from 'neuroglancer/util/array'
import {decompress_compresso} from 'neuroglancer/sliceview/compresso';
import {decodeCompresso} from 'neuroglancer/async_computation/decode_compresso_request';
import {registerAsyncComputation} from 'neuroglancer/async_computation/handler';

registerAsyncComputation(
    decodeCompresso,
    async function(data: Uint8Array) {      
      const result: TypedArray = await decompress_compresso(data);
      const cast_result = new Uint8Array(result.buffer);
      return { value: cast_result, transfer: [cast_result.buffer] };
    });