# MoHSES - REST Adapter

The REST Adapter allows for web endpoints to send and receive data on the MoHSES Data Bus without the need for a DDS connection.
- Stateless
- Supports web applications, such as...
  - Instructor Interface (load scenario, play/pause sim, change patient condition)
  - Student Interface (log in, perform certain actions, view score, feedback, and assessment)
  - Technician Interface (monitor simulation health, maintenance, troubleshoot)

### Requirements
- [MoHSES Standard Library](https://github.com/AdvancedModularManikin/amm-library) (and FastRTPS and FastCDR)
- rapidjson (`apt-get install rapidjson-dev`)
- [pistache](https://github.com/pistacheio/pistache)

### Installation
```bash
    $ git clone https://github.com/AdvancedModularManikin/rest-adapter
    $ mkdir rest-adapter/build && cd tcp-bridge/build
    $ cmake ..
    $ cmake --build . --target install
```

By default on a Linux system this will install into `/usr/local/bin`




