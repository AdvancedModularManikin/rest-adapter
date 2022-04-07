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

