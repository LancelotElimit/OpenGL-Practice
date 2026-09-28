# Asset sources

The following reference assets are downloaded by CMake from Poly Haven and
are released under the CC0 license:

- `sunset_jhbcentral_1k.hdr`
  - https://polyhaven.com/a/sunset_jhbcentral
- `concrete_diff_1k.jpg`
- `concrete_nor_gl_1k.jpg`
- `concrete_rough_1k.jpg`
- `concrete_ao_1k.jpg`
  - https://polyhaven.com/a/concrete

They are intentionally limited to 1K resolution to keep this learning project
small. The renderer falls back to its procedural environment and default
textures when optional external assets are unavailable.

`SimpleSkin.gltf` is the embedded glTF 2.0 version of Khronos Group's
SimpleSkin sample model. It is released as CC0 and is used here to demonstrate
joint animation and GPU vertex skinning:

- https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/SimpleSkin
