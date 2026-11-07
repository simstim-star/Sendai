# Sendai Renderer

DirectX 12 renderer written in pure C 99, with PBR for glTF models. This is a working in progress project that I created for educational purposes, in order to learn better about COM and DirectX 12.

Also check my ports of the official DirectX 12 samples to pure C: https://github.com/simstim-star/DirectX-Graphics-Samples-in-C

And my port of DirectX Math to pure C: https://github.com/simstim-star/DirectXMath-in-C

Currently it supports glTF with PBR, but with many limitations (only point lights, only some glTF extensions, etc).

<img width="1915" height="984" alt="image" src="https://github.com/user-attachments/assets/203dbd71-2060-43fc-aaa8-4968e2fe57a0" />


## How to build (MSVC)

Get [DirectXMath-in-C](https://github.com/simstim-star/DirectXMath-in-C) and install it with:

```
cmake -S . -B build-install
cmake --build build-install --target INSTALL
```

This will generate the folder `xmathc`, probably in `C:/`. This will allow us to use `xmathc` in other projects.

We can build this project with CMake:

```
cmake -S . -B build --debug-find-pkg=xmathc --fresh
cmake --build build
```

This will already link `xmathc` if it is installed.
