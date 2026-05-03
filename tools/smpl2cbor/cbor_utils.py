"""
Utility module for CBOR serialization and deserialization with custom type support.
Used for saving and loading SKEL models and parameters represented in CBOR format for SKELpp project compatibility.
Automatically handles NumPy arrays, SciPy sparse matrices, and Chumpy arrays during encode/decode.
Supports recursive processing of nested data structures for seamless persistence.
"""

import cbor2
import numpy as np
import scipy.sparse
try:
    import chumpy
    HAS_CHUMPY = True
except ImportError:
    HAS_CHUMPY = False
from pathlib import Path
from typing import Any, Dict, Union, IO


__all__ = ['load', 'loads', 'dump', 'dumps', 'load_file', 'save_file']


###############################################################################
# Internal Functions
###############################################################################

# NOTE: We use hard-coded type strings to ensure stability across different Python library versions.
# Dynamic type resolution (e.g., `str(type(obj))`) often exposes internal module paths (such as `scipy.sparse._coo.coo_matrix`), 
# which may change in future updates and break compatibility when deserializing a CBOR file.
_NUMPY_ARRAY_TYPE_STR = "<class 'numpy.ndarray'>"
_SCIPY_SPARSE_COO_MATRIX_TYPE_STR = "<class 'scipy.sparse.coo_matrix'>"
_CHUMPY_ARRAY_TYPE_STR = "<class 'chumpy.ch_array'>"

def _nparray_to_bytes(arr: np.ndarray) -> bytes:
    # Save array data as bytes for optimization
    # NOTE: Force `order='C'` to unify the memory layout to Row-Major. ('C' means C-order, Row-Major)
    #       even if the original is Fortran-order(Col-Major), it is rearranged into Row-Major order when converted to bytes here.
    #       https://numpy.org/devdocs/reference/generated/numpy.ndarray.tobytes.html
    return arr.tobytes(order='C')

def _encode_numpy_array(arr: np.ndarray) -> Dict[str, Any]:
    """Convert numpy.ndarray to a serializable dictionary using binary data."""
    if not isinstance(arr, np.ndarray):
        raise TypeError("Input is not a numpy.ndarray")
    
    return {
        'type': _NUMPY_ARRAY_TYPE_STR,
        'data': _nparray_to_bytes(arr),
        'dtype': arr.dtype.name,
        'shape': list(arr.shape)
    }

def _decode_numpy_array(data: Dict[str, Any]) -> np.ndarray:
    """Restore numpy.ndarray from binary dictionary information."""
    try:
        if not isinstance(data, dict):
            raise ValueError("Input data is not a dictionary")
        if data['type'] != _NUMPY_ARRAY_TYPE_STR:
            raise ValueError("Data type mismatch for numpy.ndarray")
            
        # Restore from bytes
        arr = np.frombuffer(data['data'], dtype=data['dtype'])
        arr = arr.reshape(data['shape'])
        return arr.copy() # Return a writable copy
    except (KeyError, ValueError) as e:
        raise ValueError(f"Failed to decode numpy.ndarray: {e}")

def _encode_scipy_sparse_coo_matrix(mat: scipy.sparse.spmatrix) -> Dict[str, Any]:
    """Convert any scipy sparse matrix to a serializable COO dictionary using binary data."""
    if not scipy.sparse.issparse(mat):
        raise TypeError("Input is not a scipy sparse matrix")

    # NOTE: While SciPy operations (like matrix multiplication) typically return CSR or CSC formats 
    # for computational efficiency, we enforce conversion to COO (Coordinate) format for serialization.
    # COO provides a canonical, flat representation (row, col, data) that is simplest to store 
    # and eliminates the complexity of handling compression pointers (indptr) used in CSR/CSC.
    if not isinstance(mat, scipy.sparse.coo_matrix):
        mat = mat.tocoo()

    return {
        'type': _SCIPY_SPARSE_COO_MATRIX_TYPE_STR,
        'data': _nparray_to_bytes(mat.data),      # Save values as bytes for optimization
        'row': _nparray_to_bytes(mat.row),        # Save row indices as bytes for optimization
        'col': _nparray_to_bytes(mat.col),        # Save col indices as bytes for optimization
        'dtype': mat.dtype.name,         # Data type of values (e.g., float32)
        'index_dtype': mat.row.dtype.name, # Data type of row/col indices (e.g., int32), needed for decoding
        'shape': list(mat.shape)
    }

def _decode_scipy_sparse_coo_matrix(data: Dict[str, Any]) -> scipy.sparse.coo_matrix:
    """Restore scipy.sparse.coo_matrix from binary dictionary information."""
    try:
        if not isinstance(data, dict):
            raise ValueError("Input data is not a dictionary")
        if data['type'] != _SCIPY_SPARSE_COO_MATRIX_TYPE_STR:
            raise ValueError("Data type mismatch for scipy.sparse.coo_matrix")
            
        # Restore components from bytes
        values = np.frombuffer(data['data'], dtype=data['dtype'])
        
        # Use saved index_dtype to correctly restore row/col arrays
        idx_dtype = data['index_dtype']
        rows = np.frombuffer(data['row'], dtype=idx_dtype)
        cols = np.frombuffer(data['col'], dtype=idx_dtype)
        
        shape = tuple(data['shape'])
        return scipy.sparse.coo_matrix((values, (rows, cols)), shape=shape)
    except (KeyError, ValueError) as e:
        raise ValueError(f"Failed to decode scipy.sparse.coo_matrix: {e}")

def _encode_chumpy_array(arr) -> Dict[str, Any]:
    """Convert chumpy array to a serializable dictionary."""
    if not HAS_CHUMPY:
        raise RuntimeError("chumpy is not installed")
    if not isinstance(arr, chumpy.Ch):
        raise TypeError("Input is not a chumpy array")
    
    # Convert chumpy array to numpy array (evaluate it)
    np_arr = np.array(arr)
    
    return {
        'type': _CHUMPY_ARRAY_TYPE_STR,
        'data': _nparray_to_bytes(np_arr),
        'dtype': np_arr.dtype.name,
        'shape': list(np_arr.shape)
    }

def _decode_chumpy_array(data: Dict[str, Any]):
    """Restore chumpy array from dictionary information."""
    try:
        if not isinstance(data, dict):
            raise ValueError("Input data is not a dictionary")
        if data['type'] != _CHUMPY_ARRAY_TYPE_STR:
            raise ValueError("Data type mismatch for chumpy array")
        
        # Restore as numpy array first
        arr = np.frombuffer(data['data'], dtype=data['dtype'])
        arr = arr.reshape(data['shape'])
        
        # Convert to chumpy array if available, otherwise return numpy array
        if HAS_CHUMPY:
            return chumpy.array(arr)
        else:
            return arr.copy()  # Return numpy array if chumpy not installed
    except (KeyError, ValueError) as e:
        raise ValueError(f"Failed to decode chumpy array: {e}")

def _encode_recursive(data: Any) -> Any:
    """Traverse data structure and convert NumPy/SciPy/Chumpy objects to dictionaries."""
    if isinstance(data, dict):
        return {k: _encode_recursive(v) for k, v in data.items()}
    elif isinstance(data, list):
        return [_encode_recursive(v) for v in data]
    elif isinstance(data, tuple): # Note: Tuples are encoded as lists in CBOR by default
        return tuple(_encode_recursive(v) for v in data)
    elif HAS_CHUMPY and isinstance(data, chumpy.Ch):
        return _encode_chumpy_array(data)
    elif isinstance(data, np.ndarray):
        return _encode_numpy_array(data)
    elif scipy.sparse.issparse(data):
        return _encode_scipy_sparse_coo_matrix(data)
    elif isinstance(data, (np.integer, np.floating)):
        return data.item()
    else:
        return data

def _decode_recursive(data: Any) -> Any:
    """Traverse data structure and restore NumPy/SciPy/Chumpy objects from serialized dictionaries."""
    if isinstance(data, dict):
        # Strict type checking to prevent false positives
        type_str = data.get('type')
        if type_str == _NUMPY_ARRAY_TYPE_STR:
            return _decode_numpy_array(data)
        elif type_str == _SCIPY_SPARSE_COO_MATRIX_TYPE_STR:
            return _decode_scipy_sparse_coo_matrix(data)
        elif type_str == _CHUMPY_ARRAY_TYPE_STR:
            return _decode_chumpy_array(data)
        
        return {k: _decode_recursive(v) for k, v in data.items()}
    
    elif isinstance(data, list):
        return [_decode_recursive(v) for v in data]
    
    elif isinstance(data, tuple):
        return tuple(_decode_recursive(v) for v in data)
    
    return data


###############################################################################
# Public Functions
###############################################################################

def dump(obj: Any, fp: IO[bytes]) -> None:
    """
    Encode object to CBOR format and write to file object.
    Automatically handles NumPy arrays, SciPy sparse matrices, and Chumpy arrays.
    """
    serializable_obj = _encode_recursive(obj)
    cbor2.dump(serializable_obj, fp)

def dumps(obj: Any) -> bytes:
    """
    Return object as CBOR-formatted bytes.
    """
    serializable_obj = _encode_recursive(obj)
    return cbor2.dumps(serializable_obj)

def load(fp: IO[bytes]) -> Any:
    """
    Read CBOR data from file object and restore python object.
    Automatically restores NumPy arrays, SciPy sparse matrices, and Chumpy arrays.
    """
    data = cbor2.load(fp)
    return _decode_recursive(data)

def loads(data: bytes) -> Any:
    """
    Restore python object from CBOR bytes.
    """
    decoded_data = cbor2.loads(data)
    return _decode_recursive(decoded_data)

def save_file(obj: Any, path: Union[str, Path]) -> None:
    """
    [Convenience] Save object to a file at the specified path.
    """
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, 'wb') as f: dump(obj, f)

def load_file(path: Union[str, Path]) -> Any:
    """
    [Convenience] Read file at the specified path and return object.
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")
    
    with open(path, 'rb') as f: return load(f)


###############################################################################
# Tests
###############################################################################

if __name__ == '__main__':
    # Test Scenarios
    print("--- Running cbor_utils tests ---")
    
    # 1. Prepare Data
    # Create a generic sparse matrix (CSR) to test forced COO conversion
    csr_matrix = scipy.sparse.random(5, 5, density=0.2, format='csr')
    
    original_data = {
        "id": 1,
        "name": "test_data",
        "dense_matrix": np.random.rand(3, 3).astype(np.float32),
        "sparse_matrix": csr_matrix, # Pass CSR to check auto-conversion
        "nested_list": [np.array([1, 2, 3]), "mixed"],
        "mixed_tuple": ("immutable_header", 99, np.array([10, 20]))
    }
    
    import tempfile
    with tempfile.TemporaryDirectory() as tmp_dir:
        test_file = Path(tmp_dir) / "test_output.cbor"
        
        # 2. Test Save
        print(f"Saving to {test_file}...")
        save_file(original_data, test_file)
        
        # 3. Test Load
        print(f"Loading from {test_file}...")
        loaded_data = load_file(test_file)
        
        # 4. Verify
        print("\nVerification start...")
        
        # --- Check Numpy ---
        assert np.allclose(original_data['dense_matrix'], loaded_data['dense_matrix'])
        print("[OK] Dense Matrix matches")
        
        # --- Check Sparse ----
        # Convert both to dense array for value comparison
        orig_arr = original_data['sparse_matrix'].toarray()
        load_arr = loaded_data['sparse_matrix'].toarray()
        assert np.allclose(orig_arr, load_arr)
        
        # Verify type is restored as COO (our serialization target)
        assert isinstance(loaded_data['sparse_matrix'], scipy.sparse.coo_matrix)
        print("[OK] Sparse Matrix matches (Restored as COO)")
        
        # --- Check Nested ----
        assert np.array_equal(original_data['nested_list'][0], loaded_data['nested_list'][0])
        print("[OK] Nested Array matches")
        
        # --- Check Tuple ---
        # Note: CBOR deserializes tuples as lists by default. 
        # We verify that the content is correct, even if the container type changed to list.
        loaded_tuple = loaded_data['mixed_tuple']
        orig_tuple = original_data['mixed_tuple']
        
        assert isinstance(loaded_tuple, list)  # Expecting list from CBOR
        assert loaded_tuple[0] == orig_tuple[0] # "immutable_header"
        assert loaded_tuple[1] == orig_tuple[1] # 99
        assert np.array_equal(loaded_tuple[2], orig_tuple[2]) # np.array([10, 20])
        print("[OK] Tuple content matches (Note: Restored as List)")

        print("\nAll tests passed!")
        input('Press Enter to exit...')