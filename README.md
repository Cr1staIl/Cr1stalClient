# Cr♱stal Client

A DDNet client for KoG gores servers.

- Download the latest test build: https://github.com/Cr1staIl/Cr1stalClient/releases/download/test-latest/Cr1stalClient-Setup.exe
- Windows installer built by GitHub Actions on every push (`.github/workflows/test-build.yml`).

## Building

```
git clone --recursive https://github.com/Cr1staIl/Cr1stalClient
cd Cr1stalClient
mkdir build && cd build
cmake ..
cmake --build .
```

## Licenses

The code is based on DDNet and uses code of other open source clients. Their copyright
and license notices are kept in `license.txt` and `LICENSE`. These files have to stay with
every copy of the client, they are part of the licenses.
