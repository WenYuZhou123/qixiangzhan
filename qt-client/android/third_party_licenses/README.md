# Android TLS Runtime

The Android build packages OpenSSL 3.0.17 arm64-v8a shared libraries as
`libcrypto_3.so` and `libssl_3.so`. Qt 6.7's OpenSSL TLS backend loads these
names at runtime for HTTPS and WSS support.

The corresponding OpenSSL license is in `OpenSSL-3.0.17-LICENSE.txt`.
