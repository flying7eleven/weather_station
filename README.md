# Weather Station Firmware
This repository contains the firmware for the [Solar Powered weather station](https://www.instructables.com/id/Solar-Powered-WiFi-Weather-Station-V20/)
project by [Open Green Energy](https://www.instructables.com/member/Open%20Green%20Energy/). In its original form, it is based on the code written
by [3KUdelta](https://github.com/3KUdelta/Solar_WiFi_Weather_Station).

The code was restructured and partly rewritten to fulfill my own requirements.

## Development
The easiest way of using this project is the [PlatformIO](https://platformio.org/) IDE. The project is fully configured with a project definition which can be used
to fetch all required libraries and build the firmware.

### Code signing
TODO

Generate a CA
```bash
openssl req -new -x509 -days 3650 -extensions v3_ca -keyout cert/ca.key.pem -out cert/ca.crt.pem
openssl x509 -outform der -in cert/ca.crt.pem -out cert/ca.crt.der.
xxd -i cert/ca.crt.der
```

Generate a developer certificate
```bash
openssl genrsa -out cert/developer.key.pem 2048
openssl req -out cert/developer.csr.pem -key cert/developer.key.pem -new
```

Sign the developer certificate
```bash
openssl x509 -req -in cert/developer.csr.pem -CA cert/ca.crt.pem -CAkey cert/ca.key.pem -CAcreateserial -out cert/developer.crt.pem -days 365
openssl x509 -outform der -in cert/developer.crt.pem -out cert/developer.crt.der.
cp cert/developer.crt.der /data/developer.crt.der
```

```bash
openssl dgst -sha256 -sign cert/developer.key.pem -out data/sig256 data/data.txt
```

### Prerequisites
TODO

### How to build
TODO
