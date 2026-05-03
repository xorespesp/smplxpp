#pragma once
#ifndef SMPLX_HOST
#  ifdef __CUDACC__
#    define SMPLX_HOST __host__
#  else
#    define SMPLX_HOST
#  endif
#endif

#include "smplx/version.hh"

#include <Eigen/Core>
#include <Eigen/Sparse>

namespace smplx
{
    using Matrix2f = Eigen::Matrix2f;
    using Matrix3f = Eigen::Matrix3f;
    using Matrix4f = Eigen::Matrix4f;
    using Vector2f = Eigen::Vector2f;
    using Vector3f = Eigen::Vector3f;
    using Vector4f = Eigen::Vector4f;

    using Scalar = float;
    using Index = uint32_t;
    using Points = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using PointsUVN = Eigen::Matrix<Scalar, Eigen::Dynamic, 8, Eigen::RowMajor>;
    using PointsRGB = Eigen::Matrix<Scalar, Eigen::Dynamic, 6, Eigen::RowMajor>;
    using Points2D = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;

    using Matrix = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
    using MatrixColMajor = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;

    using Triangles = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;

    using SparseMatrixColMajor = Eigen::SparseMatrix<Scalar>;
    using SparseMatrix = Eigen::SparseMatrix<Scalar, Eigen::RowMajor>;

    enum class Gender {
        unknown, neutral, male, female
    };

} // namespace smplx