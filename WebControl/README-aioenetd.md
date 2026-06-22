# aioenetd WebControl integration notes

This tree adds the Linux/aioenetd WebControl path without importing the STM32/NetX or ETH-DIO-48A device shims.

Builds of `aioenetd` now include:

- `WebControl/webctl_core.c`
- `WebControl/webctl_assets.c`
- `WebControl/webctl_posix_linux.cpp`
- `WebControl/webctl_aioenetd.cpp`

The POSIX transport serves filesystem content first and falls back to the embedded packed assets:

1. `${AIOENETD_WEB_ROOT}` if set
2. `/home/acces/www`
3. `WebControl/webctl_assets.inc`

Runtime environment knobs:

- `AIOENETD_WEB_PORT`, default `80`
- `AIOENETD_WEB_ROOT`, default `/home/acces/www`
- `AIOENETD_WEB_AUTH_ENABLE`, default `1`
- `AIOENETD_WEB_AUTH_USERNAME`, default `admin`
- `AIOENETD_WEB_AUTH_PASSWORD`, default `admin`
- `AIOENETD_WEB_AUTH_REALM`, default `aioenetd`

Initial supported endpoints:

- `GET /`
- `GET /api/v1/status`
- `GET /api/v1/capabilities`
- `GET /api/v1/system`
- `GET /api/v1/io`
- `POST /api/v1/io/outputs`
- `POST /api/v1/io/direction`
- `GET /api/v1/network`
- `GET /api/v1/https`
- `GET /api/v1/certificates`

Network configuration writes, HTTPS, certificates, ADC dashboard routes, and DAC dashboard routes remain intentionally unimplemented in this first pass.
