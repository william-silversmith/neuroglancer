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

#include <turbojpeg.h>
#include <stdbool.h>
#include <stdio.h>

long int jpeg_image_bytes(
	unsigned char* buf, 
	unsigned int num_bytes,
	bool convert_to_grayscale
) {
	tjhandle decompressor = tjInitDecompress();
	int jpegSubsamp = 0;
	int jpegColorspace = 0;
	int width = 0;
	int height = 0;

	int error = tjDecompressHeader3(
		decompressor, 
		buf, num_bytes, 
		&width, &height, 
		&jpegSubsamp, &jpegColorspace
	);
	tjDestroy(decompressor);

	long int num_channels = 3;
	if (jpegColorspace == TJPF_GRAY || convert_to_grayscale) {
		num_channels = 1;
	}

	return (long int)width * (long int)height * num_channels;
}

int jpeg_decompress(
	unsigned char* buf, unsigned int num_bytes, 
	void* out, unsigned int num_out_bytes,
	bool convert_to_grayscale
) {
	tjhandle decompressor = tjInitDecompress();
	int error = 0;

	int jpegSubsamp = 0;
	int jpegColorspace = 0;
	int width = 0;
	int height = 0;

	error = tjDecompressHeader3(
		decompressor, 
		buf, num_bytes, 
		&width, &height, 
		&jpegSubsamp, &jpegColorspace
	);
	if (error) {
		printf("%s", tjGetErrorStr2(decompressor));
		goto DONE;
	}

	unsigned int num_channels = (jpegColorspace == TJPF_GRAY) 
		? 1 
		: 3;

	if (convert_to_grayscale) {
		jpegColorspace = TJPF_GRAY;
	}

	if (num_out_bytes != (long)width * (long)height * (long)num_channels) {
		error = 100;
		goto DONE;
	}

	error = tjDecompress2(
		decompressor, 
		buf, num_bytes, 
		out, width, /*pitch=*/0, height, 
		jpegColorspace, TJFLAG_FASTDCT
	);

	DONE:
	tjDestroy(decompressor);
	return error;
}