/* This path is not compiled directly.
 *
 * Upstream stb_image.h is stored as zlib-compressed base64 fragments
 * (stb_image_b64_XX.txt) and amalgamated into the CMake build directory as
 * generated/stb_image.h (see amalgamate.py).
 *
 * Source: https://github.com/nothings/stb
 */
#error "Include the CMake-generated stb_image.h (build/generated), not third_party/stb/stb_image.h"
