### Prerequisites

- [RapidJSON](https://github.com/miloyip/rapidjson) - JSON parser/generator for C++  
    Debian:
    ```bash
    $ sudo apt install rapidjson-dev
    ```
- [Pistache](https://github.com/pistacheio/pistache) - Simple C++ REST framework


### Installation
```bash
    $ git clone https://github.com/AdvancedModularManikin/rest-adapter
    $ mkdir rest-adapter/build && cd rest-adapter/build
    $ cmake ..
    $ cmake --build . --target install
```

### Creating a .deb package
```bash
    $ cd build
    $ cpack -G DEB
```

### REST Adapter routes
The REST adapter exposes the following routes:
```
/nodes            - retrieve the current state of all node paths
/node/<name>      - retrieve a single node_path
/command/<action> - issue a command
/actions	  - retrieve a list of all available actions
/states		  - retrieve a list of all available starting states / scenarios
/patients	  - retrieve a list of all available patients
/modules      - retrieve a list of all connected modules and their statuses/capabilities
/module/<id>  - retrieve a single module's status, configuration and capabilities
```

#### Examples: 
```
http://localhost:9080/node/Cardiovascular_HeartRate
http://localhost:9080/command/[SYS]START_SIM
http://localhost:9080/command/[SYS]LOAD_SCENARIO:m1s1
```
