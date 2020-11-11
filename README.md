# AppleBacklightSmoother

[![Github release](https://img.shields.io/github/release/hieplpvip/AppleBacklightSmoother.svg?color=blue)](https://github.com/hieplpvip/AppleBacklightSmoother/releases/latest)
[![Github downloads](https://img.shields.io/github/downloads/hieplpvip/AppleBacklightSmoother/total.svg?color=blue)](https://github.com/hieplpvip/AppleBacklightSmoother/releases)
[![Build Status](https://travis-ci.com/hieplpvip/AppleBacklightSmoother.svg?branch=master)](https://travis-ci.com/hieplpvip/AppleBacklightSmoother)
[![Scan Status](https://scan.coverity.com/projects/21839/badge.svg)](https://scan.coverity.com/projects/21839)
[![Donate with PayPal](https://img.shields.io/badge/paypal-donate-red.svg)](https://paypal.me/lebhiep)

A Lilu plugin that patches Apple Intel graphics drivers to get smooth backlight transition

#### Installation

- Compile `SSDT-PNLF.dsl`.
- Inject `AppleBacklightSmoother.kext` and `SSDT-PNLF.aml` using your favorite bootloader.
- If you're using Coffee Lake or above, you need to turn off WhateverGreen's backlight patch by using boot argument `igfxcflbklt=0`.

#### Boot arguments

- `-applbklsmoothdbg` to enable debug printing (available in DEBUG binaries).
- `-applbklsmoothbeta` to enable loading on unsupported macOS versions (11.0 and below are enabled by default).
- `-applbklsmoothoff` to disable kext loading.
- `igfxpwmmax=0x????` to set PWMMAX value to `0x????`

#### Credits

- [Apple](https://www.apple.com) for macOS
- [vit9696](https://github.com/vit9696) for [Lilu](https://github.com/acidanthera/Lilu) and [WhateverGreen](https://github.com/acidanthera/WhateverGreen)
