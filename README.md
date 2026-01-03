# Sendai Engine
A game engine based on DirectX12 in pure C. This is a working in progress project that I created for educational purposes, in order to learn better about COM and DirectX.

Also check my ports of the official DirectX12 samples to pure C: https://github.com/simstim-star/DirectX-Graphics-Samples-in-C
And my port of DirectX Match to pure C: https://github.com/simstim-star/DirectXMath-in-C

Currently it is very rudimentary, only allowing very basic gltf loading:
<img width="1919" height="874" alt="image" src="https://github.com/user-attachments/assets/849fe52d-d70b-4d6d-b4d2-449fc46e2821" />


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
