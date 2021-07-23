# golang-insecureskipverify-patch
Simple patcher tool to turn off TLS handshake validation in golang binaries

If you need to inspect TLS protected communication of a black-box golang binary and it does not trust the system level CA certificates,
then you can use this tool to patch the executable to act like `InsecureSkipVerify` was turned on. (You still have some additional work,
configuring a transparent proxy and setting up mitmproxy or alike)

## Payloads

We are replacing a conditional jump to a JMP+NOP in crypto/tls.(*Conn).verifyServerCertificate:

 0x5a4bc5 <crypto/tls.(*Conn).verifyServerCertificate+133>       jne    0x5a5225 <crypto/tls.(*Conn).verifyServerCertificate+1765>   

This line is effectively the same as:

```
if !c.config.InsecureSkipVerify {
```

if InsecureSkipVerify is false, it does not jump. We shall jump always. This is:

```
  5a4ba1:       48 8b 8c 24 88 02 00    mov    0x288(%rsp),%rcx
  5a4ba8:       00
  5a4ba9:       48 85 c9                test   %rcx,%rcx
  5a4bac:       0f 8f 7e 06 00 00       jg     5a5230 <crypto/tls.(*Conn).verifyServerCertificate+0x6f0>
  5a4bb2:       48 8b 9c 24 78 02 00    mov    0x278(%rsp),%rbx
  5a4bb9:       00
  5a4bba:       48 8b 73 48             mov    0x48(%rbx),%rsi
  5a4bbe:       80 be a0 00 00 00 00    cmpb   $0x0,0xa0(%rsi)
  5a4bc5:       0f 85 5a 06 00 00       jne    5a5225 <crypto/tls.(*Conn).verifyServerCertificate+0x6e5>  <- this is to be patched
  5a4bcb:       48 8b 76 10             mov    0x10(%rsi),%rsi
  5a4bcf:       48 85 f6                test   %rsi,%rsi
  5a4bd2:       0f 84 41 06 00 00       je     5a5219 <crypto/tls.(*Conn).verifyServerCertificate+0x6d9>
  5a4bd8:       48 8b 06                mov    (%rsi),%rax
  5a4bdb:       48 89 f2                mov    %rsi,%rdx
  5a4bde:       66 90                   xchg   %ax,%ax
  5a4be0:       ff d0                   callq  *%rax
  5a4be2:       48 8b 04 24             mov    (%rsp),%rax
```

The pre-calculated payloads you can use (before after pairs):

go116-amd64: 0f855a060000 e95b06000090
go113-amd64: 0f8532050000 e93305000090


## Examples

You may use the example go app included in this repo. Normally, it's output looks like this:

```
# ./gohttp
Sending an HTTP request
Error while fetching https://untrusted-root.badssl.com/: Get https://untrusted-root.badssl.com/: x509: certificate signed by unknown authority
```

Patching it:

```
root@f4297f8cd9e5:/data/2/go-insecuretls-patch/example-goapp# ../filepatcher/filepatcher.pl gohttp 0f8532050000 e93305000090
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
