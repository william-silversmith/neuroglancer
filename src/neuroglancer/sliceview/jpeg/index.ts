/**
 * @license
 * Copyright 2022 William Silvermsith
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

import libjpegturboWasmDataUrl from './libjpegturbo.wasm';


let tempRet0:any = 0;
const libraryEnv = {
  emscripten_notify_memory_growth: function () {},
  proc_exit: (code: number) => {
    throw `proc exit: ${code}`;
  },
  getTempRet0: function () { return tempRet0; },
  setTempRet0: function (val) { tempRet0 = val; },
  _emscripten_throw_longjmp: function () { throw "longjmp" },
  environ_get: function () {},
  environ_sizes_get: function () {},
  fd_close: function () { return 0; },
  fd_seek: function () {},
  fd_write: function () {},
  invoke_i: function () {},
  invoke_ii: function () {},
  invoke_iii: function () {},
  invoke_iiii: function () {},
  invoke_vi: function () {},
  invoke_viii: function () {},
};

const jpegModulePromise = (async () => {
  const response = await fetch(libjpegturboWasmDataUrl);
  const wasmCode = await response.arrayBuffer();
  const m = await WebAssembly.instantiate(wasmCode, {
    env: libraryEnv,
    wasi_snapshot_preview1: libraryEnv,
  });
  (m.instance.exports._initialize as Function)();
  return m;
})();

export async function decompressJpeg(
  buffer: Uint8Array, width: number, height: number, 
  numComponents: number, convertToGrayscale: boolean
) : Promise<Uint8Array> {

  console.log("WOW!!!", jpegModulePromise);
  
  const m = await jpegModulePromise;

  const nbytes = width * height * numComponents;

  // console.log("WOW", m.instance.exports.jpeg_image_bytes as);
  // const nbytes = (m.instance.exports.jpeg_num_bytes as Function)(
  //   buffer, buffer.byteLength, convertToGrayscale
  // );
  // console.log("WOW2");

  // let numChannels = numComponents;
  // if (convertToGrayscale) {
  //   numChannels = 1;
  // }

  if (nbytes != width * height * numComponents) {
    throw new Error(
      `jpeg: Image decode parameters did not match expected chunk parameters.
         Expected: width: ${width} height: ${height} channels: ${numComponents}
         Decoded Length:  ${nbytes} bytes
         Convert to Grayscale? ${convertToGrayscale}
        `
    );
  }

  if (nbytes < 0) {
    throw new Error(`jpeg: Failed to decode jpeg image size. image size: ${nbytes}`);
  }

  // heap must be referenced after creating bufPtr and imagePtr because
  // memory growth can detatch the buffer.
  let bufPtr = (m.instance.exports.malloc as Function)(buffer.byteLength);
  const imagePtr = (m.instance.exports.malloc as Function)(nbytes);
  let heap = new Uint8Array((m.instance.exports.memory as WebAssembly.Memory).buffer);
  heap.set(buffer, bufPtr);

  const code = (m.instance.exports.jpeg_decompress as Function)(
    bufPtr, buffer.byteLength, imagePtr, nbytes, convertToGrayscale
  );

  try {
    if (code !== 0) {
      throw new Error(`jpeg: Failed to decode jpeg image. decoder code: ${code}`);
    }

    // Likewise, we reference memory.buffer instead of heap.buffer
    // because memory growth during decompress could have detached
    // the buffer.
    const image = new Uint8Array(
      (m.instance.exports.memory as WebAssembly.Memory).buffer,
      imagePtr, nbytes
    );
    // copy the array so it can be memory managed by JS
    // and we can free the emscripten buffer
    return image.slice(0);
  }
  finally {
    (m.instance.exports.free as Function)(bufPtr);
    (m.instance.exports.free as Function)(imagePtr);      
  }
}
