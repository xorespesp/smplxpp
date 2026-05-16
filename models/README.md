## Obtaining the model files

Each body model is distributed under its own license, so the original `.pkl` / `.npz` files must be downloaded from the project sites and converted locally to the `.cbor` format this library consumes.  
The converter lives in `tools/smpl2cbor/` and is invoked through uv:

```bash
$ uv run --project tools/smpl2cbor tools/smpl2cbor/smpl2cbor.py path/to/MODEL.pkl [MORE.pkl ...]
```

Run the commands below from the project root. Converted files go under `models/<model>/`.

## SMPL-X
1. Register at http://smpl-x.is.tue.mpg.de.
2. Under Downloads, grab the "download SMPL-X v..." archive.
3. From the archive, extract the `models/smplx/SMPLX_*.pkl` files.
4. Convert every gender in one call, for example:
   ```bash
   $ uv run --project tools/smpl2cbor tools/smpl2cbor/smpl2cbor.py path/to/SMPLX_NEUTRAL.pkl path/to/SMPLX_MALE.pkl path/to/SMPLX_FEMALE.pkl
   ```
5. Place the resulting `.cbor` files under `models/smplx/`.

At this point you should have:
- `models/smplx/SMPLX_NEUTRAL.cbor`
- `models/smplx/SMPLX_MALE.cbor`
- `models/smplx/SMPLX_FEMALE.cbor`

## SMPL
1. Register at http://smpl.is.tue.mpg.de.
2. Under Downloads, take the "for Python users" archive.
3. Unpack it into the project root so that a `smpl` folder sits next to `CMakeLists.txt`.
4. Convert the models depending on the release you downloaded:
   - SMPL 1.0:
     ```bash
     $ uv run --project tools/smpl2cbor tools/smpl2cbor/smpl2cbor.py smpl/models/basicModel_f_lbs_10_207_0_v1.0.0.pkl smpl/models/basicmodel_m_lbs_10_207_0_v1.0.0.pkl
     ```
   - SMPL 1.1:
     ```bash
     $ uv run --project tools/smpl2cbor tools/smpl2cbor/smpl2cbor.py smpl/models/basicModel_f_lbs_10_207_0_v1.1.0.pkl smpl/models/basicmodel_m_lbs_10_207_0_v1.1.0.pkl smpl/models/basicmodel_neutral_lbs_10_207_0_v1.1.0.pkl
     ```

You should now have:
- `models/smpl/SMPL_MALE.cbor`
- `models/smpl/SMPL_FEMALE.cbor`

### Neutral model
Starting with SMPL 1.1 the neutral model ships inside the SMPL release, so the steps below are only needed for the older 1.0 model.

1. Register at http://smplify.is.tue.mpg.de.
2. Download the archive listed under "Code and model".
3. Unpack it into the project root so that a `smplify_public` folder sits next to `CMakeLists.txt`.
4. Convert the neutral model:
   ```bash
   $ uv run --project tools/smpl2cbor tools/smpl2cbor/smpl2cbor.py smplify_public/code/models/basicModel_neutral_lbs_10_207_0_v1.0.0.pkl
   ```

You should now have:
- `models/smpl/SMPL_NEUTRAL.cbor`

## SMPL+H
1. Register at http://mano.is.tue.mpg.de.
2. Under Downloads, get "Extended SMPLH model for AMASS".
3. Unpack it with `tar xf /path/to/smplh.tar.xz`.
4. Pull the `.pkl` model file out of each gender directory.
5. Convert them, then move the results into `models/smplh/` as `SMPLH_<GENDER>.cbor`.

You should now have:
- `models/smplh/SMPLH_NEUTRAL.cbor`
- `models/smplh/SMPLH_MALE.cbor`
- `models/smplh/SMPLH_FEMALE.cbor`

## Notes
- The SMPL / SMPL-H UV maps come from [github.com/radvani], shared in this
  thread: https://github.com/Lotayou/densebody_pytorch/issues/4
- The SMPL-X UV map is custom-made and admittedly rough.
- To supply your own UV map, edit `models/<model>/uv.txt`. Its format is:
    - line 1: `n`, the number of UV vertices
    - next `n` lines: `u v`, the float coordinates of each vertex
    - following `num_faces` lines (one per row of the model's `f` array):
      `a b c`, the 1-based indices of the UV vertices making up each triangle
