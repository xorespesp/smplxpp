#pragma once
#include "smplxpp/defs.hh"

#include <Eigen/Geometry>
#include <iostream>
#include <filesystem>
#include <optional>

#define _SMPLXPP_ASSERT(x)                                                 \
    do {                                                                 \
        if (!(x)) {                                                      \
            std::cerr << "smplxpp assertion FAILED: \"" << #x << "\" ("    \
                      << (bool)(x) << ")\n  at " << __FILE__ << " line " \
                      << __LINE__ << "\n";                               \
            std::exit(1);                                                \
        }                                                                \
    } while (0)
#define _SMPLXPP_ASSERT_EQ(x, y)                                            \
    do {                                                                  \
        if ((x) != (y)) {                                                 \
            std::cerr << "smplxpp assertion FAILED: " << #x << " == " << #y \
                      << " (" << (x) << " != " << (y) << ")\n  at "       \
                      << __FILE__ << " line " << __LINE__ << "\n";        \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)
#define _SMPLXPP_ASSERT_NE(x, y)                                            \
    do {                                                                  \
        if ((x) == (y)) {                                                 \
            std::cerr << "smplxpp assertion FAILED: " << #x << " != " << #y \
                      << " (" << (x) << " == " << (y) << ")\n  at "       \
                      << __FILE__ << " line " << __LINE__ << "\n";        \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)
#define _SMPLXPP_ASSERT_LE(x, y)                                                 \
    do {                                                                       \
        if ((x) > (y)) {                                                       \
            std::cerr << "smplxpp assertion FAILED: " << #x << " <= " << #y      \
                      << " (" << (x) << " > " << (y) << ")\n  at " << __FILE__ \
                      << " line " << __LINE__ << "\n";                         \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)
#define _SMPLXPP_ASSERT_LT(x, y)                                           \
    do {                                                                 \
        if ((x) >= (y)) {                                                \
            std::cerr << "smplxpp assertion FAILED: " << #x << " < " << #y \
                      << " (" << (x) << " >= " << (y) << ")\n  at "      \
                      << __FILE__ << " line " << __LINE__ << "\n";       \
            std::exit(1);                                                \
        }                                                                \
    } while (0)

#include <chrono>
#define _SMPLXPP_BEGIN_PROFILE \
    auto start = std::chrono::high_resolution_clock::now()
#define _SMPLXPP_PROFILE(x)                                                      \
    do {                                                                       \
        double _delta = std::chrono::duration<double, std::milli>(             \
                            std::chrono::high_resolution_clock::now() - start) \
                            .count();                                          \
        printf("%s: %f ms = %f fps\n", #x, _delta, 1e3f / _delta);             \
        start = std::chrono::high_resolution_clock::now();                     \
    } while (false)
#define _SMPLXPP_PROFILE_STEPS(x, stp)                                  \
    do {                                                              \
        printf("%s: %f ms / step\n", #x,                              \
               std::chrono::duration<double, std::milli>(             \
                   std::chrono::high_resolution_clock::now() - start) \
                       .count() /                                     \
                   (stp));                                            \
        start = std::chrono::high_resolution_clock::now();            \
    } while (false)

namespace smplxpp::util
{
    // Angle-axis to rotation matrix using custom implementation
    template <class T, int Option = Eigen::ColMajor>
    [[nodiscard]] inline Eigen::Matrix<T, 3, 3, Option> rodrigues(
        const Eigen::Ref<const Eigen::Matrix<T, 3, 1>>& vec) {
        const T theta = vec.norm();
        const Eigen::Matrix<T, 3, 3, Option> eye =
            Eigen::Matrix<T, 3, 3, Option>::Identity();

        if (std::fabs(theta) < 1e-5f)
            return eye;
        else {
            const T c = std::cos(theta);
            const T s = std::sin(theta);
            const Eigen::Matrix<T, 3, 1> r = vec / theta;
            Eigen::Matrix<T, 3, 3, Option> skew;
            skew << 0, -r.z(), r.y(), r.z(), 0, -r.x(), -r.y(), r.x(), 0;
            return c * eye + (1 - c) * r * r.transpose() + s * skew;
        }
    }

    // Angle-axis to rotation matrix through Eigen quaternion
    // (slightly slower than rodrigues, not useful)
    template <class T, int Option = Eigen::ColMajor>
    [[nodiscard]] inline Eigen::Matrix<T, 3, 3, Option> rodrigues_eigen(
        const Eigen::Ref<const Eigen::Matrix<T, 3, 1>>& vec) {
        return Eigen::template AngleAxis<T>(vec.norm(), vec / vec.norm())
            .toRotationMatrix();
    }

    // Affine transformation matrix (hopefully) faster multiplication
    // bottom row omitted
    template <class T, int Option = Eigen::ColMajor>
    inline void mul_affine(
        const Eigen::Ref<const Eigen::Matrix<T, 3, 4, Option>>& a,
        Eigen::Ref<Eigen::Matrix<T, 3, 4, Option>> b) {
        b.template leftCols<3>() =
            a.template leftCols<3>() * b.template leftCols<3>();
        b.template rightCols<1>() =
            a.template rightCols<1>() +
            a.template leftCols<3>() * b.template rightCols<1>();
    }

    // Affine transformation matrix 'in-place' inverse
    template <class T, int Option = Eigen::ColMajor>
    inline void inv_affine(Eigen::Ref<Eigen::Matrix<T, 3, 4, Option>> a) {
        a.template leftCols<3>() = a.template leftCols<3>().inverse();
        a.template rightCols<1>() =
            -a.template leftCols<3>() * a.template rightCols<1>();
    }

    // Homogeneous transformation matrix in-place inverse
    template <class T, int Option = Eigen::ColMajor>
    inline void inv_homogeneous(Eigen::Ref<Eigen::Matrix<T, 3, 4, Option>> a) {
        a.template leftCols<3>().transposeInPlace();
        a.template rightCols<1>() =
            -a.template leftCols<3>() * a.template rightCols<1>();
    }

    [[nodiscard]] inline const char* gender_to_str(Gender gender) {
        switch (gender) {
        case Gender::neutral: return "NEUTRAL";
        case Gender::male: return "MALE";
        case Gender::female: return "FEMALE";
        default: return "UNKNOWN";
        }
    }

    [[nodiscard]] inline Gender parse_gender(std::string str) {
        for (auto& c : str) c = static_cast<char>(std::toupper(c));
        if (str == "NEUTRAL") return Gender::neutral;
        if (str == "MALE") return Gender::male;
        if (str == "FEMALE") return Gender::female;
        std::cerr << "WARNING: Gender '" << str << "' could not be parsed\n";
        return Gender::unknown;
    }

    // Path resolve helpers
    template <typename ModelConfig>
    [[nodiscard]] inline std::optional<std::filesystem::path> find_model_file(
        const std::filesystem::path& models_dir_path,
        Gender model_gender_type)
    {
        std::filesystem::path model_file_path =
            models_dir_path / (std::string{ ModelConfig::default_path_prefix } + gender_to_str(model_gender_type) + ".cbor");

        if (std::error_code ec;
            !std::filesystem::is_regular_file(model_file_path, ec)) {
            return std::nullopt;
        }

        return model_file_path;
    }

    template <typename ModelConfig>
    [[nodiscard]] inline std::optional<std::filesystem::path> find_model_uv_file(
        const std::filesystem::path& models_dir_path)
    {
        std::filesystem::path uv_file_path =
            models_dir_path / std::string{ ModelConfig::default_uv_path };

        if (std::error_code ec; 
            !std::filesystem::is_regular_file(uv_file_path, ec)) {
            return std::nullopt;
        }

        return uv_file_path;
    }

} // namespace smplxpp::util