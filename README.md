# smplxpp
SMPL-X C++ implementation in Eigen and CUDA.  
AMASS integration included.

### Dependencies
- CMake 3.24+
- C++17 compatible compiler
- uv for Python tools and model conversion
- OpenGL 4.6 (Optional, for build the viewer)
- CUDA Toolkit with nvcc supporting C++17 (Optional, for GPU acceleration)

### Getting the SMPL/SMPL+H/SMPL-X Models
- See [models/README.md](https://github.com/xorespesp/smplxpp/tree/master/models)

### Building
> **NOTE:** Currently tested on Windows 11 (VS2022).

```bash
$ mkdir build && cd build
$ cmake .. -G Ninja
$ ninja
$ cmake --build . --config Release
$ ninja -j<number-of-threads>
$ sudo ninja install
```

### License
This library is licensed under Apache v2 (non-copyleft).
However, remember that the following are non-commercial research-only:
- SMPL
- SMPL-X
- Mano/SMPL+H
- AMASS dataset
- SMPLify

### References
<a id="1">[1]</a> SMPL: http://smpl.is.tue.mpg.de.
SMPL: A Skinned Multi-Person Linear Model.  Matthew Loper, Naureen Mahmood, Javier Romero, Gerard Pons-Moll, Michael J. Black. 2015

<a id="2">[2]</a> SMPLify: http://smplify.is.tue.mpg.de.
Keep it SMPL: Automatic Estimation of 3D Human Pose and Shape from a Single Image.
Federica Bogo*, Angjoo Kanazawa*, Christoph Lassner, Peter Gehler, Javier Romero, Michael Black.
2016

<a id="3">[3]</a> Mano: http://mano.is.tue.mpg.de.
Embodied Hands: Modeling and Capturing Hands and Bodies Together. Javier Romero*, Dimitrios Tzionas*, and Michael J Black. 2017

<a id="4">[4]</a> SMPL-X: http://smpl-x.is.tue.mpg.de.
Expressive Body Capture: 3D Hands, Face, and Body from a Single Image. G. Pavlakos*, V. Choutas*, N. Ghorbani, T. Bolkart, A. A. A. Osman, D. Tzionas and M. J. Black. 2019

<a id="5">[5]</a> AMASS: https://amass.is.tue.mpg.de/.
AMASS: Archive of Motion Capture as Surface Shapes.
Mahmood, Naureen and Ghorbani, Nima and Troje, Nikolaus F. and Pons-Moll, Gerard and Black, Michael J.