# Third-party provenance

- `firmware/src/usb_link.c` adapts the CDC descriptor layout from
  [libopencm3-examples usb_cdcacm](https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/stm32-h103/usb_cdcacm/cdcacm.c),
  Copyright 2010 Gareth McMullin, **LGPL-3.0-or-later**.
  The original notice is retained. Licence texts are included in
  `firmware/licenses/`. Dependencies retain their own licences.
- libopencm3 is installed through the pinned PlatformIO configuration.
  USB IDs 0483:5740 are inherited demonstration IDs for a local prototype,
  not a registered product identity.
- [toane/renault_remote](https://github.com/toane/renault_remote) informed earlier
  study of contact-scanning methods. Its source and images are **not** bundled
  here. No explicit licence was found in the studied snapshot.
- `docs/images/connector-reference.png` is a third-party photograph supplied by
  the user. The original photographer and article URL are not established;
  no photo licence is asserted. Any numbered version is a derivative reference
  annotation, not a newly authored photograph.
- A project-wide licence for the original scanner, decoder, and documentation
  has not yet been selected. No blanket licence is implied for third-party files.
