# XyzCpp
An implementation of the Xyz-protocol in C++.
This library is header-only (though linking with appropriate shared libraries is required).
Uses:
- boost.asio (networking)
- boost.iostreams (deflation/inflation with zlib module)
- zlib
- openssl (for sha256, and md5, TODO: find standalone libraries for these)
- libecc (for ecdh, openssl documentation for ECDH is *awful*)