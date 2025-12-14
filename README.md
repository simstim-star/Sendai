# Sendai Engine
A game engine based on DirectX12 in pure C. This is a working in progress project that I created for educational purposes, in order to learn better about COM and DirectX.

Also check my ports of the official DirectX12 samples to pure C: https://github.com/simstim-star/DirectX-Graphics-Samples-in-C
And my port of DirectX Match to pure C: https://github.com/simstim-star/DirectXMath-in-C

Currently it is very rudimentary, only allowing very basic gltf loading:
<img width="1917" height="726" alt="image" src="https://github.com/user-attachments/assets/782f4f6e-a095-41a4-8738-a3cb164425a8" />


## How to build (MSVC)

Get [DirectXMath-in-C](https://github.com/simstim-star/DirectXMath-in-C) and install it with:

```
cmake -S . -B build-install
cmake --build build-install --target INSTALL
```

This will generate the folder `xmathc`, probably in `C:/`. This will allow us to use `xmathc` in other projects.

Now we can build this project with:

```
cmake -S . -B build --debug-find-pkg=xmathc --fresh
cmake --build build
```

This will already link `xmathc`.
