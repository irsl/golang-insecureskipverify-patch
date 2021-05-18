# golang-insecureskipverify-patch
Simple patcher tool to turn off TLS handshake validation in golang binaries

If you need to inspect TLS protected communication of a black-box golang binary and it does not trust the system level CA certificates,
then you can use this tool to patch the executable to act like `InsecureSkipVerify` was turned on. (You still have some additional work,
configuring a transparent proxy and setting up mitmproxy or alike)
