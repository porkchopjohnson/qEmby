# Fedora KDE security build

This fork targets **Fedora 44 x86_64 / KDE Plasma**, using Fedora's Qt 6,
libmpv, and QtKeychain libraries. The application reports version
`0.0.8-security.1`; the RPM release is `1.security.fc44`.

## Security changes

- Emby/Jellyfin session tokens and per-server proxy passwords live in the OS
  wallet through QtKeychain (KDE Wallet on Plasma), instead of `servers.json`.
  Global proxy passwords also use the wallet.
- Wallet entries are bound to the server URL and user identity. Changing a
  profile's destination does not retrieve the original destination's token.
- **No plaintext wallet fallback.** If access is denied or the wallet is
  unavailable, new credentials remain usable in memory for the current session
  but may need to be entered again next launch. Wallet operations time out after
  30 seconds. Wallet access can display KDE's normal unlock/access prompt.
- Legacy plaintext server settings are migrated and scrubbed on load, even if
  the wallet is unavailable. Server settings are written atomically with `0600`
  permissions. Global proxy passwords migrate when proxy settings are loaded.
- WebSocket logs no longer include authentication query parameters. Both file
  and console/journal Qt logs redact URL query strings, URL passwords, common
  token/password assignments, and the local stream relay's bearer path.
- The login form defaults to HTTPS. Public HTTP server connections are rejected;
  HTTP remains available for localhost, RFC1918 IPv4 addresses, and IPv6 ULA
  addresses. A LAN hostname requires HTTPS; use its private IP for local HTTP.
  Local HTTP is still unencrypted and should only be used on trusted networks.
- Core API requests use same-origin redirects, preventing credentials from
  following redirects to a different host or port. Some unusual server/proxy
  setups may need their final API address entered directly.
- The embedded mpv player enables TLS certificate verification explicitly.
- Fedora builds use distro-managed runtime libraries and ELF hardening
  (PIE/ASLR, stack protection, FORTIFY, RELRO/NOW, non-executable stack).
- Fetched QCoro and spdlog sources are pinned to commit hashes. The upstream
  QWindowKit submodule remains pinned to its existing commit.
- Upstream installer updates are disabled for the Fedora build, so they cannot
  overwrite these changes. Rebuild/install a new security RPM to update.

This is targeted hardening, not a complete security audit or a sandbox. Optional
danmaku integrations still use third-party services and their existing custom
provider credential settings; WebDAV retains its existing encrypted profile
storage. Previously copied settings, old logs, backups, and previously exported
cloud bundles are not retroactively scrubbed. Revoke previously exposed tokens
on the media server. Server login credentials are intentionally not included in
new configuration sync exports; sign in again on each device.

## Install the built package

```bash
sudo dnf install ./build-fedora/qemby-0.0.8-1.security.fc44.x86_64.rpm
```

Launch **qEmby** from KDE's application launcher, or run `qemby`.
The RPM is locally built and unsigned. Its checksum can be verified against
`build-fedora/SHA256SUMS` when provided alongside it.

## Build from source

Enable an appropriate Fedora/RPM Fusion repository providing `mpv-devel` if it
is not available in your current repositories. Then:

```bash
sudo dnf install git gcc-c++ cmake ninja-build rpm-build pkgconf-pkg-config \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qttools-devel \
  qt6-qtwebsockets-devel qt6-qtwayland qtkeychain-qt6-devel mpv-devel
git submodule update --init --recursive
cmake -S . -B build-fedora -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64 \
  -DQEMBY_BUILD_TESTS=ON
cmake --build build-fedora --parallel 6
ctest --test-dir build-fedora --output-on-failure
cpack --config build-fedora/CPackConfig.cmake -B build-fedora
```

The regression suite uses an unavailable D-Bus session to ensure it cannot
access your wallet. To verify the real wallet explicitly, run these as separate
processes in a KDE session (they create and remove one disposable test entry):

```bash
build-fedora/bin/wallet-probe write
build-fedora/bin/wallet-probe read
build-fedora/bin/wallet-probe delete
build-fedora/bin/wallet-probe absent
```

The CMake helpers and desktop/RPM packaging are included in this fork because
the upstream release source referenced helpers that were not published. Three
malformed Chinese translation entries were also repaired so `lrelease` succeeds.
