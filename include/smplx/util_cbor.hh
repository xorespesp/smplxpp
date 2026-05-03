#pragma once
#include "smplx/defs.hh"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace smplx::util
{
    class nparray_object;
    class sparse_object;

    class cbor_object {
    public:
        cbor_object();
        cbor_object(const cbor_object& other);
        cbor_object(cbor_object&& other) noexcept;
        cbor_object& operator=(const cbor_object& other);
        cbor_object& operator=(cbor_object&& other) noexcept;
        virtual ~cbor_object();

        // Load from file (CBOR format)
        static cbor_object load(const std::filesystem::path& path);

        // Check if key exists
        bool contains(const std::string& key) const;

        // Access element
        cbor_object operator[](const std::string& key) const;
        cbor_object at(const std::string& key) const;

        // Convert to specialized objects
        nparray_object as_nparray() const;
        sparse_object as_sparse_object() const;

        // Get value as basic type
        template <typename T>
        T get() const;

        // Access raw binary data if this node is a byte string (e.g. 'data' field)
        const std::vector<uint8_t>& get_binary() const;

    protected:
        struct Impl;
        std::shared_ptr<Impl> _impl;

        explicit cbor_object(std::shared_ptr<Impl> impl);
    };

    class nparray_object : public cbor_object {
    public:
        nparray_object();
        explicit nparray_object(const cbor_object& obj);
        nparray_object(const nparray_object& other);
        nparray_object(nparray_object&& other) noexcept;
        nparray_object& operator=(const nparray_object& other);
        nparray_object& operator=(nparray_object&& other) noexcept;
        ~nparray_object() override;

        // Check if the holding value is a numpy array dictionary (or chumpy array)
        bool is_numpy_array() const;

        std::string get_dtype() const;
        std::vector<size_t> get_shape() const;
        const std::vector<uint8_t>& get_data() const;
    };

    inline constexpr size_t ANY_SHAPE = static_cast<size_t>(-1);

    // Helper functions to interpret nparray_object as matrix
    // These allow reshaping if the total number of elements matches.
    Matrix as_float_matrix(const nparray_object& obj, size_t rows = ANY_SHAPE, size_t cols = ANY_SHAPE);

    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
    as_uint_matrix(const nparray_object& obj, size_t rows = ANY_SHAPE, size_t cols = ANY_SHAPE);

    class sparse_object : public cbor_object {
    public:
        sparse_object();
        explicit sparse_object(const cbor_object& obj);
        sparse_object(const sparse_object& other);
        sparse_object(sparse_object&& other) noexcept;
        sparse_object& operator=(const sparse_object& other);
        sparse_object& operator=(sparse_object&& other) noexcept;
        ~sparse_object() override;

        // Check if the holding value is a sparse matrix dictionary
        bool is_sparse_matrix() const;

        // Load as sparse float matrix
        // Expects custom sparse dictionary format (data, row, col)
        Eigen::SparseMatrix<float> as_sparse_float_matrix(size_t rows = ANY_SHAPE, size_t cols = ANY_SHAPE) const;

        std::vector<size_t> get_shape() const;
    };

    // Independent assert shape helper
    void assert_shape(const std::vector<size_t>& actual, std::initializer_list<size_t> expected);

} // namespace smplx::util