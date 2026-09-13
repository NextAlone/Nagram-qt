# Nagram Desktop branding

Nagram Desktop is an independent Telegram client maintained by NextAlone.
It is based on Telegram Desktop; upstream source, history, license and
copyright notices remain with their respective authors.

The Nagram name and project identity are controlled by NextAlone.
Nagram icon artwork is Copyright © MaitungTM. All rights reserved.
The artwork is a separate brand asset; the source-code license does not
license its use as the branding of third-party distributions. Preserve this
notice with copied or derived icon assets.

The canonical icon is the Nagram iOS layered artwork, retained in
`Telegram/Resources/branding/Nagram.icon`. Its desktop rendition is exported
with Xcode Icon Composer into `nagram.png`; application PNGs, the Windows ICO,
macOS icon sets and non-macOS tray glyphs are derived from this artwork.
Desktop PNG renditions use 8-bit sRGB with alpha for platform compatibility.
Required upstream and third-party notices remain unchanged.

The macOS menu bar retains the upstream Telegram paper-plane glyph and unread
badge rendering, including its sizing and appearance handling.

Application identity:

- Display name: Nagram Desktop; executable and macOS bundle: Nagram.
- macOS and Linux application ID: `xyz.nextalone.nagram.desktop`.
- Windows application ID: `NextAlone.NagramDesktop` with a distinct installer
  ID and notification activator.
- macOS data directory: `Application Support/Nagram Desktop`; Linux uses
  `NagramDesktop` under the system application data directory. Existing
  Telegram profiles are not imported automatically.
- Portable directory: `NagramForcePortable`.
- Telegram `tg:` and `tonsite:` protocol compatibility is retained.

Upstream automatic updates and crash submission are disabled at build time.
Manual release links point to this repository. Store distribution is not
configured; desktop builds are supported. Internal build targets, resource
bundle names and Telegram service/protocol names remain compatible with the
upstream build system. Signing, distribution credentials and an independent
update service are separate release work.
