# golang-insecureskipverify-patch
Simple patcher tool to turn off TLS handshake validation in golang binaries

If you need to inspect TLS protected communication of a black-box golang binary and it does not trust the system level CA certificates,
then you can use this tool to patch the executable to act like `InsecureSkipVerify` was turned on. (You still have some additional work,
configuring a transparent proxy and setting up mitmproxy or alike)

## Examples

You may use the example go app included in this repo. Normally, it's output looks like this:

```
# ./gohttp
Sending an HTTP request
Error while fetching https://untrusted-root.badssl.com/: Get https://untrusted-root.badssl.com/: x509: certificate signed by unknown authority
```

Patching it:

```
root@f4297f8cd9e5:/data/2/go-insecuretls-patch/example-goapp# ../golang-insecureskipverify-patcher.pl gohttp go113-amd64
Looking for: 0f8532050000
1 match!
Replacing to: e93305000090
0019e245
ready, file patched in place
```

Then restart:

```
root@f4297f8cd9e5:/data/2/go-insecuretls-patch/example-goapp# ./gohttp
Sending an HTTP request
Succeeded: HTTP/1.1 200 OK
Transfer-Encoding: chunked
Cache-Control: no-store
Connection: keep-alive
```

As an alternative, or if you don't prefer making changes in the executable, you may use the mempatcher tool which is also included in this project.

```
root@f4297f8cd9e5:/data/mempatcher# ps uax|grep gohttp
root      6391  2.0  0.1 1069664 17920 pts/1   Sl+  23:04   0:00 ./gohttp
```

Patching (note the patch payloads are coming from the original perl patcher script):

```
root@f4297f8cd9e5:/data/mempatcher# ./mempatcher 6391 0f8532050000 e93305000090
...
Found a match at 0x59e245
After adjusting to a word boundary, it is 0x59e240
process memory before:
  0000  98 00 00 00 00 0f 85 32 05 00 00                 .......2...
process memory after:
  0000  98 00 00 00 00 e9 33 05 00 00 90                 ......3....
PTRACE_ATTACH succeeded
PTRACE_POKETEXT succeeded
last round with PTRACE_POKETEXT, need PTRACE_PEEKTEXT
last PTRACE_POKETEXT succeeded
PTRACE_DETACH succeeded
Success
```
