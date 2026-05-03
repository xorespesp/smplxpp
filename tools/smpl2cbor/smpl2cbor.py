# smpl2cbor.py
import sys, pickle, argparse
import numpy as np
import cbor_utils
from pathlib import Path
from typing import *

def smpl2cbor_main(
    smpl_model_path: Path, 
    output_path: Optional[Path] = None
) -> None:
    smpl_model_path = Path(smpl_model_path)
    
    # Determine file type by extension
    file_ext = smpl_model_path.suffix.lower()
    
    if file_ext == '.npz':
        # Load NPZ file (SMPL-X model)
        npz_data = np.load(smpl_model_path, allow_pickle=True)
        model_dict = dict(npz_data)
        npz_data.close()
    elif file_ext == '.pkl':
        # Load PKL file (SMPL model)
        with open(smpl_model_path, 'rb') as model_file:
            try:
                # Open the pickle using latin1 encoding
                model_dict = pickle.load(model_file, encoding='latin1')
            except:
                model_dict = pickle.load(model_file)
    else:
        raise ValueError(f"Unsupported file format: {file_ext}. Only .pkl and .npz are supported.")
    
    output_cbor_file_data: bytes = cbor_utils.dumps(model_dict)

    if output_path is None:
        output_path: Path = (smpl_model_path.parent / f'{smpl_model_path.stem}.cbor').absolute().resolve()
    else:
        output_path = Path(output_path).absolute().resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f'Writing to: {str(output_path)}')
    with open(output_path, 'wb') as output_file:
        output_file.write(output_cbor_file_data)
    print('\nAll done.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Util to convert SMPL/SMPL-X model file (.pkl or .npz) to cbor format.',
        epilog='Tip: Download SMPL models from https://smpl.is.tue.mpg.de/ or SMPL-X models from https://smpl-x.is.tue.mpg.de/'
    )
    parser.add_argument('input', type=str, help='Path to SMPL/SMPL-X model file (.pkl or .npz)')
    parser.add_argument('-o', '--output', type=str, default=None, help='Output path for the cbor file (default: same directory as input with .cbor extension)')
    args = parser.parse_args()
    smpl2cbor_main(args.input, args.output)
