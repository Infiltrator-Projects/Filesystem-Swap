# Filesystem-Swap
Independent filesystem implementation for InfiltratorOS, using the shared Common ABI.

## Installer files

- `/System/Filesystems/SWAP.so`
- `/System/Resources/Documentation/Filesystem-Swap/Swap.pdf`

The current package builds the shared object and generates the PDF. A standalone kernel module is pending.

InfiltratorOS calls `--system-package-mode` and decides which outputs need updating. Its worker builds a missing or older SO, repairs the PDF independently, and stores the matching released kernel module when available. Use `--build-only --output DIRECTORY` to build a Debian package.
