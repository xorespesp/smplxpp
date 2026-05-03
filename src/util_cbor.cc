#include "smplx/util_cbor.hh"
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace smplx::util
{
    using json = nlohmann::json;

    ////////////////////////////////////////////////////////////////////////////////
    // --- cbor_object::Impl ---
    struct cbor_object::Impl {
        json _json;

        Impl() = default;
        explicit Impl(json j) : _json(std::move(j)) {}
    };

    ////////////////////////////////////////////////////////////////////////////////
    // --- Internal Helper ---
    namespace {
        template <typename T>
        const T* get_data_ptr(const nlohmann::json& j) {
            if (!j.contains("data") || !j["data"].is_binary()) {
                throw std::runtime_error("flattened data not found or not binary");
            }
            const auto& bin = j["data"].get_binary();
            return reinterpret_cast<const T*>(bin.data());
        }
    } // namespace

    ////////////////////////////////////////////////////////////////////////////////
    // --- cbor_object Implementation ---

    cbor_object::cbor_object() : _impl(std::make_shared<Impl>()) {}

    cbor_object::cbor_object(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}

    cbor_object::cbor_object(const cbor_object& other) : _impl(other._impl) {}

    cbor_object::cbor_object(cbor_object&& other) noexcept : _impl(std::move(other._impl)) {}

    cbor_object& cbor_object::operator=(const cbor_object& other) {
        if (this != &other) {
            _impl = other._impl;
        }
        return *this;
    }

    cbor_object& cbor_object::operator=(cbor_object&& other) noexcept {
        if (this != &other) {
            _impl = std::move(other._impl);
        }
        return *this;
    }

    cbor_object::~cbor_object() = default;

    cbor_object cbor_object::load(const std::filesystem::path& path) {
        std::ifstream i(path, std::ios::binary);
        if (!i) {
            throw std::runtime_error("cbor_object::load: Could not open file: " + path.generic_string());
        }
        std::vector<uint8_t> bytes(
            (std::istreambuf_iterator<char>(i)),
            (std::istreambuf_iterator<char>()));

        if (bytes.empty()) {
            throw std::runtime_error("cbor_object::load: File is empty: " + path.generic_string());
        }

        try {
            json j = json::from_cbor(bytes);
            auto impl = std::make_shared<Impl>(std::move(j));
            return cbor_object(impl);
        }
        catch (const json::parse_error& e) {
            throw std::runtime_error(std::string("cbor_object::load: CBOR parse error: ") + e.what());
        }
    }

    bool cbor_object::contains(const std::string& key) const {
        if (!_impl || !_impl->_json.is_object()) return false;
        return _impl->_json.contains(key);
    }

    cbor_object cbor_object::operator[](const std::string& key) const {
        if (!_impl || !_impl->_json.is_object()) {
            return cbor_object(std::make_shared<Impl>(json(nullptr)));
        }
        if (_impl->_json.contains(key)) {
            return cbor_object(std::make_shared<Impl>(_impl->_json[key]));
        }
        return cbor_object(std::make_shared<Impl>(json(nullptr)));
    }

    cbor_object cbor_object::at(const std::string& key) const {
        if (!_impl || !_impl->_json.contains(key)) {
            throw std::out_of_range("cbor_object::at: key not found: " + key);
        }
        return cbor_object(std::make_shared<Impl>(_impl->_json.at(key)));
    }

    const std::vector<uint8_t>& cbor_object::get_binary() const {
        if (!_impl) throw std::runtime_error("cbor_object::get_binary: null implementation");
        return _impl->_json.get_binary();
    }

    template <typename T>
    T cbor_object::get() const {
        if (!_impl) throw std::runtime_error("cbor_object::get: null implementation");
        return _impl->_json.get<T>();
    }

    // Explicit instantiations for common types
    template std::string cbor_object::get<std::string>() const;
    template int cbor_object::get<int>() const;
    template float cbor_object::get<float>() const;
    template double cbor_object::get<double>() const;
    template bool cbor_object::get<bool>() const;
    template std::vector<size_t> cbor_object::get<std::vector<size_t>>() const;

    nparray_object cbor_object::as_nparray() const {
        return nparray_object(*this);
    }

    sparse_object cbor_object::as_sparse_object() const {
        return sparse_object(*this);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // --- nparray_object Implementation ---

    nparray_object::nparray_object() : cbor_object() {}

    nparray_object::nparray_object(const cbor_object& obj) : cbor_object(obj) {}

    nparray_object::nparray_object(const nparray_object& other) : cbor_object(other) {}

    nparray_object::nparray_object(nparray_object&& other) noexcept : cbor_object(std::move(other)) {}

    nparray_object& nparray_object::operator=(const nparray_object& other) {
        if (this != &other) {
            cbor_object::operator=(other);
        }
        return *this;
    }

    nparray_object& nparray_object::operator=(nparray_object&& other) noexcept {
        if (this != &other) {
            cbor_object::operator=(std::move(other));
        }
        return *this;
    }

    nparray_object::~nparray_object() = default;

    bool nparray_object::is_numpy_array() const {
        if (!_impl || !_impl->_json.is_object() || !_impl->_json.contains("type")) { return false; }
        const std::string t = _impl->_json["type"].get<std::string>();
        return t == "<class 'numpy.ndarray'>" || t == "<class 'chumpy.ch_array'>";
    }

    std::vector<size_t> nparray_object::get_shape() const {
        if (is_numpy_array() && _impl && _impl->_json.contains("shape")) {
            return _impl->_json["shape"].get<std::vector<size_t>>();
        }
        return {};
    }

    std::string nparray_object::get_dtype() const {
        if (!is_numpy_array() || !_impl) return "";
        return _impl->_json.at("dtype").get<std::string>();
    }

    const std::vector<uint8_t>& nparray_object::get_data() const {
        if (!is_numpy_array()) throw std::runtime_error("nparray_object::get_data: Not a numpy array");
        if (!_impl) throw std::runtime_error("nparray_object::get_data: null implementation");
        // Ensure "data" field exists and is binary
        if (!_impl->_json.contains("data") || !_impl->_json["data"].is_binary()) {
            throw std::runtime_error("nparray_object::get_data: data binary field not found");
        }
        return _impl->_json["data"].get_binary();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // --- sparse_object Implementation ---

    sparse_object::sparse_object() : cbor_object() {}

    sparse_object::sparse_object(const cbor_object& obj) : cbor_object(obj) {}

    sparse_object::sparse_object(const sparse_object& other) : cbor_object(other) {}

    sparse_object::sparse_object(sparse_object&& other) noexcept : cbor_object(std::move(other)) {}

    sparse_object& sparse_object::operator=(const sparse_object& other) {
        if (this != &other) {
            cbor_object::operator=(other);
        }
        return *this;
    }

    sparse_object& sparse_object::operator=(sparse_object&& other) noexcept {
        if (this != &other) {
            cbor_object::operator=(std::move(other));
        }
        return *this;
    }

    sparse_object::~sparse_object() = default;

    bool sparse_object::is_sparse_matrix() const {
        if (!_impl || !_impl->_json.is_object() || !_impl->_json.contains("type")) return false;
        std::string t = _impl->_json["type"];
        return t == "<class 'scipy.sparse.coo_matrix'>";
    }

    std::vector<size_t> sparse_object::get_shape() const {
         if (is_sparse_matrix() && _impl && _impl->_json.contains("shape")) {
            return _impl->_json["shape"].get<std::vector<size_t>>();
        }
        return {};
    }

    Eigen::SparseMatrix<float> sparse_object::as_sparse_float_matrix(size_t rows, size_t cols) const {
        if (!is_sparse_matrix()) throw std::runtime_error("as_sparse_float_matrix: Not a sparse COO matrix");
        if (!_impl) throw std::runtime_error("as_sparse_float_matrix: null implementation");

        std::vector<size_t> shape = get_shape();
        if (shape.size() != 2) throw std::runtime_error("Sparse matrix must be 2D");
        if (rows != ANY_SHAPE && rows != shape[0]) throw std::runtime_error("as_sparse_float_matrix: Row mismatch");
        if (cols != ANY_SHAPE && cols != shape[1]) throw std::runtime_error("as_sparse_float_matrix: Col mismatch");

        size_t R = shape[0];
        size_t C = shape[1];

        std::string idx_dtype = _impl->_json.at("index_dtype").get<std::string>();
        std::string val_dtype = _impl->_json.at("dtype").get<std::string>();

        const auto& row_bin = _impl->_json.at("row").get_binary();
        const auto& col_bin = _impl->_json.at("col").get_binary();
        const auto& data_bin = _impl->_json.at("data").get_binary();

        auto read_indices = [&](const std::vector<uint8_t>& bin, std::string_view type) -> std::vector<int> {
            size_t count = 0;
            std::vector<int> out;
            if (type == "int32" || type == "<i4") {
                count = bin.size() / 4;
                out.resize(count);
                const int32_t* ptr = reinterpret_cast<const int32_t*>(bin.data());
                for (size_t i = 0; i < count; ++i) out[i] = (int)ptr[i];
            }
            else if (type == "int64" || type == "<i8") {
                count = bin.size() / 8;
                out.resize(count);
                const int64_t* ptr = reinterpret_cast<const int64_t*>(bin.data());
                for (size_t i = 0; i < count; ++i) out[i] = (int)ptr[i];
            }
            else {
                throw std::runtime_error("Unsupported index dtype: " + std::string(type));
            }
            return out;
        };

        auto read_values = [&](const std::vector<uint8_t>& bin, std::string_view type, size_t count) -> std::vector<float> {
            std::vector<float> out(count);
            if (type == "float32" || type == "<f4") {
                if (bin.size() != count * 4) throw std::runtime_error("Data size mismatch");
                const float* ptr = reinterpret_cast<const float*>(bin.data());
                for (size_t i = 0; i < count; ++i) out[i] = ptr[i];
            }
            else if (type == "float64" || type == "<f8") {
                if (bin.size() != count * 8) throw std::runtime_error("Data size mismatch");
                const double* ptr = reinterpret_cast<const double*>(bin.data());
                for (size_t i = 0; i < count; ++i) out[i] = (float)ptr[i];
            }
            else {
                throw std::runtime_error("Unsupported value dtype: " + std::string(type));
            }
            return out;
        };

        std::vector<int> r_idx = read_indices(row_bin, idx_dtype);
        std::vector<int> c_idx = read_indices(col_bin, idx_dtype);
        size_t nnz = r_idx.size();
        if (c_idx.size() != nnz) throw std::runtime_error("Row and Col index count mismatch");

        std::vector<float> vals = read_values(data_bin, val_dtype, nnz);

        std::vector<Eigen::Triplet<float>> triplets;
        triplets.reserve(nnz);
        for (size_t i = 0; i < nnz; ++i) {
            triplets.emplace_back(r_idx[i], c_idx[i], vals[i]);
        }

        Eigen::SparseMatrix<float> mat(R, C);
        mat.setFromTriplets(triplets.begin(), triplets.end());
        return mat;
    }
    
    Matrix as_float_matrix(const nparray_object& obj, size_t rows, size_t cols) {
        if (!obj.is_numpy_array()) throw std::runtime_error("as_float_matrix: Not a numpy array");

        std::vector<size_t> shape = obj.get_shape();
        size_t total_elements = 1;
        for (auto s : shape) total_elements *= s;

        // If target shape is specified, check against total elements
        if (rows != ANY_SHAPE && cols != ANY_SHAPE) {
            if (rows * cols != total_elements) {
                throw std::runtime_error("as_float_matrix: Element count mismatch. Expected " + std::to_string(rows * cols) + ", got " + std::to_string(total_elements));
            }
        }
        else {
            if (rows == ANY_SHAPE) rows = shape.size() > 0 ? shape[0] : 0;
            if (cols == ANY_SHAPE) {
                if (shape.size() > 2) {
                    throw std::runtime_error("as_float_matrix: Cannot infer 2D shape from >2D array without explicit dimensions");
                }
                cols = shape.size() > 1 ? shape[1] : 1;
            }
            if (rows * cols != total_elements) {
                 throw std::runtime_error("as_float_matrix: Inferred shape mismatch");
            }
        }

        Matrix res(rows, cols);
        std::string dtype = obj.get_dtype();

        const auto& bin = obj.get_data();

        if (dtype == "float32" || dtype == "<f4") {
            if (bin.size() != total_elements * sizeof(float)) throw std::runtime_error("as_float_matrix: binary size mismatch (float32)");
            const float* ptr = reinterpret_cast<const float*>(bin.data());
            Eigen::Map<const Matrix> map(ptr, rows, cols);
            res = map;
        }
        else if (dtype == "float64" || dtype == "<f8") {
             if (bin.size() != total_elements * sizeof(double)) throw std::runtime_error("as_float_matrix: binary size mismatch (float64)");
            const double* ptr = reinterpret_cast<const double*>(bin.data());
            Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> map(ptr, rows, cols);
            res = map.cast<float>();
        }
        else {
            throw std::runtime_error("as_float_matrix: Unsupported dtype: " + dtype);
        }
        return res;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // --- helpers implementation ---

    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
        as_uint_matrix(const nparray_object& obj, size_t rows, size_t cols)
    {
        if (!obj.is_numpy_array()) throw std::runtime_error("as_uint_matrix: Not a numpy array");

        std::vector<size_t> shape = obj.get_shape();
        size_t total_elements = 1;
        for (auto s : shape) total_elements *= s;

        if (rows != ANY_SHAPE && cols != ANY_SHAPE) {
             if (rows * cols != total_elements) {
                throw std::runtime_error("as_uint_matrix: Element count mismatch");
            }
        } else {
             if (rows == ANY_SHAPE) rows = shape.size() > 0 ? shape[0] : 0;
             if (cols == ANY_SHAPE) {
                 if (shape.size() > 2) throw std::runtime_error("as_uint_matrix: Cannot infer 2D shape from >2D array");
                 cols = shape.size() > 1 ? shape[1] : 1;
             }
             if (rows * cols != total_elements) throw std::runtime_error("as_uint_matrix: Inferred shape mismatch");
        }

        Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> res(rows, cols);
        std::string dtype = obj.get_dtype();
        const auto& bin = obj.get_data();

        if (dtype == "uint32" || dtype == "<u4") {
            if (bin.size() != total_elements * 4) throw std::runtime_error("as_uint_matrix: binary size mismatch");
            const uint32_t* ptr = reinterpret_cast<const uint32_t*>(bin.data());
            Eigen::Map<const Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> map(ptr, rows, cols);
            res = map.cast<Index>();
        }
        else if (dtype == "int32" || dtype == "<i4") {
             if (bin.size() != total_elements * 4) throw std::runtime_error("as_uint_matrix: binary size mismatch");
            const int32_t* ptr = reinterpret_cast<const int32_t*>(bin.data());
             Eigen::Map<const Eigen::Matrix<int32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> map(ptr, rows, cols);
            res = map.cast<Index>();
        }
        else if (dtype == "int64" || dtype == "<i8") {
             if (bin.size() != total_elements * 8) throw std::runtime_error("as_uint_matrix: binary size mismatch");
            const int64_t* ptr = reinterpret_cast<const int64_t*>(bin.data());
             Eigen::Map<const Eigen::Matrix<int64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> map(ptr, rows, cols);
            res = map.cast<Index>();
        }
        else {
            throw std::runtime_error("as_uint_matrix: Unsupported dtype: " + dtype);
        }
        return res;
    }

    void assert_shape(const std::vector<size_t>& actual, std::initializer_list<size_t> expected) {
        std::vector<size_t> exp_vec = expected;
        if (exp_vec.size() == 1 && exp_vec[0] == ANY_SHAPE) return;

        if (actual.size() != exp_vec.size()) {
            std::cerr << "smplx::util::assert_shape failed: Rank mismatch. Actual: " << actual.size() << ", Expected: " << exp_vec.size() << std::endl;
            std::exit(1);
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            if (exp_vec[i] != ANY_SHAPE && actual[i] != exp_vec[i]) {
                std::cerr << "smplx::util::assert_shape failed: Dimension " << i << " mismatch. Actual: " << actual[i] << ", Expected: " << exp_vec[i] << std::endl;
                std::exit(1);
            }
        }
    }

} // namespace 
