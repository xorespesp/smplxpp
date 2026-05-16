# smplxpp
SMPL-X C++ implementation in Eigen and CUDA.  
AMASS integration included.

## Dependencies
- CMake 3.24+
- C++17 compatible compiler
- uv for Python tools and model conversion
- [triengine](https://github.com/xorespesp/triengine) (Optional, for building the examples)
- CUDA Toolkit with nvcc supporting C++17 (Optional, for GPU acceleration)

## Getting Started

### 1. Install the model files

The SMPL / SMPL+H / SMPL-X models are required at runtime and are not bundled.  
See [models/README.md](models/README.md) for how to download and install them.

### 2. Build the project
> **NOTE:** Currently tested on VS2022, CMake 3.24+ with Ninja.

Configure and build with one of the presets in `CMakePresets.json`:

```bash
cmake --preset win64-release-static-examples
cmake --build out/build/win64-release-static-examples
```

Preset names follow `win64-{debug,release}-{static,shared}-{minimum,examples}[-cuda]`:
- `minimum` builds only the library; `examples` also builds `example-basic` and `example-amass`.
- Append `-cuda` to enable the CUDA backend (requires a CUDA toolkit).

To install:

```bash
cmake --install out/build/win64-release-static-examples
```

## License
This library is a modification of [sxyu/smplxpp](https://github.com/sxyu/smplxpp) by Alex Yu, licensed under Apache v2 (non-copyleft; see [LICENSE](LICENSE)).  
Keep in mind, though, that the underlying models and datasets listed below carry their own non-commercial, research-only terms:
- SMPL
- SMPL-X
- MANO/SMPL+H
- AMASS dataset
- SMPLify

## References
[1] **[SMPL](http://smpl.is.tue.mpg.de)** -
*SMPL: A Skinned Multi-Person Linear Model.*
Matthew Loper, Naureen Mahmood, Javier Romero, Gerard Pons-Moll, Michael J. Black. 2015.

[2] **[SMPLify](http://smplify.is.tue.mpg.de)** -
*Keep it SMPL: Automatic Estimation of 3D Human Pose and Shape from a Single Image.*
Federica Bogo, Angjoo Kanazawa, Christoph Lassner, Peter Gehler, Javier Romero, Michael J. Black. 2016.

[3] **[MANO](http://mano.is.tue.mpg.de)** -
*Embodied Hands: Modeling and Capturing Hands and Bodies Together.*
Javier Romero, Dimitrios Tzionas, Michael J. Black. 2017.

[4] **[SMPL-X](http://smpl-x.is.tue.mpg.de)** -
*Expressive Body Capture: 3D Hands, Face, and Body from a Single Image.*
G. Pavlakos, V. Choutas, N. Ghorbani, T. Bolkart, A. A. A. Osman, D. Tzionas, M. J. Black. 2019.

[5] **[AMASS](https://amass.is.tue.mpg.de/)** -
*AMASS: Archive of Motion Capture as Surface Shapes.*
Naureen Mahmood, Nima Ghorbani, Nikolaus F. Troje, Gerard Pons-Moll, Michael J. Black. 2019.