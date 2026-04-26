# TODO

## Documentation

- [ ] Create a storage (e.g. shared drive or repository release) where anyone can download:
  - Quartus 22.1 installer
  - ARM toolchain (`arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf`)
  - Ubuntu 22.04 LTS image
- [ ] If feasible, provide a pre-configured VM (Ubuntu 22.04 + Quartus + toolchain already installed and ready to use)
- [ ] Document the full list of `apt` packages required on Ubuntu 22.04 to build the project (kernel, U-Boot, Buildroot, mkimage, etc.)
- [ ] Review and finalize `README.md`

## Kernel

- [ ] Enable LED Heartbeat Trigger in `make menuconfig`:
  `Device Drivers → LED Support → LED Trigger support → LED Heartbeat Trigger`

## System configuration

- [ ] Write and document a `chrony` configuration file
