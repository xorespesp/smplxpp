#include "smpl_sequence.hh"

#include <cnpy.h>
#include <smplx/util.hh>
#include <fstream>
#include <iostream>
#include <vector>

namespace detail {
    inline constexpr size_t ANY_SHAPE = static_cast<size_t>(-1);

    inline void assert_shape(const cnpy::NpyArray& m,
                      std::initializer_list<size_t> shape) {
        _SMPLX_ASSERT_EQ(m.shape.size(), shape.size());
        size_t idx = 0;
        for (auto& dim : shape) {
            if (dim != ANY_SHAPE)
                _SMPLX_ASSERT_EQ(m.shape[idx], dim);
            ++idx;
        }
    }

    inline smplx::Matrix load_float_matrix(const cnpy::NpyArray& raw, size_t r, size_t c) {
        size_t dwidth = raw.word_size;
        _SMPLX_ASSERT(dwidth == 4 || dwidth == 8);
        if (raw.fortran_order) {
            if (dwidth == 4) {
                return Eigen::template Map<const Eigen::Matrix<
                    float, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>(
                           raw.data<float>(), r, c)
                    .template cast<smplx::Scalar>();
            } else {
                return Eigen::template Map<const Eigen::Matrix<
                    double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>(
                           raw.data<double>(), r, c)
                    .template cast<smplx::Scalar>();
            }
        } else {
            if (dwidth == 4) {
                return Eigen::template Map<const Eigen::Matrix<
                    float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(
                           raw.data<float>(), r, c)
                    .template cast<smplx::Scalar>();
            } else {
                return Eigen::template Map<const Eigen::Matrix<
                    double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(
                           raw.data<double>(), r, c)
                    .template cast<smplx::Scalar>();
            }
        }
    }

    inline Eigen::Matrix<smplx::Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
    load_uint_matrix(const cnpy::NpyArray& raw, size_t r, size_t c) {
        size_t dwidth = raw.word_size;
        _SMPLX_ASSERT(dwidth == 4 || dwidth == 8);
        if (raw.fortran_order) {
            if (dwidth == 4) {
                return Eigen::template Map<const Eigen::Matrix<
                    uint32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>(
                           raw.data<uint32_t>(), r, c)
                    .template cast<smplx::Index>();
            } else {
                return Eigen::template Map<const Eigen::Matrix<
                    uint64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>(
                           raw.data<uint64_t>(), r, c)
                    .template cast<smplx::Index>();
            }
        } else {
            if (dwidth == 4) {
                return Eigen::template Map<const Eigen::Matrix<
                    uint32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(
                           raw.data<uint32_t>(), r, c)
                    .template cast<smplx::Index>();
            } else {
                return Eigen::template Map<const Eigen::Matrix<
                    uint64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(
                           raw.data<uint64_t>(), r, c)
                    .template cast<smplx::Index>();
            }
        }
    }
} // namespace

// AMASS npz structure
// 'trans':           (#frames, 3)
// 'gender':          str
// 'mocap_framerate': float
// 'betas':           (16)              first 10 are from the usual SMPL
// 'dmpls':           (#frames, 8)      soft tissue
// 'poses':           (#frames, 156)    first 66 are SMPL joint parameters
// excluding hand.
//                                      last 90 are MANO joint parameters, which
//                                          correspond to last 90 joints in
//                                          SMPL-X (NOT hand PCA)
//
template <class SequenceConfig>
Sequence<SequenceConfig>::Sequence(const std::filesystem::path& path) {
    if (!path.empty()) {
        load(path);
    } else {
        n_frames = 0;
        gender = smplx::Gender::neutral;
    }
}

template <class SequenceConfig>
bool Sequence<SequenceConfig>::load(const std::filesystem::path& path) {
    if (!std::ifstream(path)) {
        n_frames = 0;
        gender = smplx::Gender::unknown;
        std::cerr << "WARNING: Sequence '" << path.generic_string()
                  << "' does not exist, loaded empty sequence\n";
        return false;
    }
    // ** READ NPZ **
    cnpy::npz_t npz = cnpy::npz_load(path.string());

    if (npz.count("trans") != 1 || npz.count("poses") != 1 ||
        npz.count("betas") != 1) {
        n_frames = 0;
        gender = smplx::Gender::unknown;
        std::cerr << "WARNING: Sequence '" << path.generic_string()
                  << "' is invalid, loaded empty sequence\n";
        return false;
    }
    auto& trans_raw = npz["trans"];
    detail::assert_shape(trans_raw, { detail::ANY_SHAPE, 3 });
    n_frames = trans_raw.shape[0];
    trans = detail::load_float_matrix(trans_raw, n_frames, 3);

    auto& poses_raw = npz["poses"];
    detail::assert_shape(poses_raw, { n_frames, SequenceConfig::n_pose_params() });
    pose = detail::load_float_matrix(poses_raw, n_frames,
                                   SequenceConfig::n_pose_params());

    auto& shape_raw = npz["betas"];
    detail::assert_shape(shape_raw, { SequenceConfig::n_shape_params() });
    shape = detail::load_float_matrix(shape_raw, SequenceConfig::n_shape_params(), 1);

    if constexpr (SequenceConfig::n_dmpls() > 0) {
        if (npz.count("dmpls") == 1) {
            auto& dmpls_raw = npz["dmpls"];
            detail::assert_shape(dmpls_raw, {n_frames, SequenceConfig::n_dmpls()});
            dmpls = detail::load_float_matrix(dmpls_raw, n_frames,
                                            SequenceConfig::n_dmpls());
        }
    }

    if (npz.count("gender")) {
        char gender_spec = npz["gender"].data_holder[0];
        gender =
            gender_spec == 'f'
                ? smplx::Gender::female
                : gender_spec == 'm'
                      ? smplx::Gender::male
                      : gender_spec == 'n' ? smplx::Gender::neutral : smplx::Gender::unknown;
    } else {
        // Default to neutral
        std::cerr << "WARNING: gender not present in '" << path.generic_string()
                  << "', using neutral\n";
        gender = smplx::Gender::neutral;
    }

    if (npz.count("mocap_framerate")) {
        auto& mocap_frate_raw = npz["mocap_framerate"];
        if (mocap_frate_raw.word_size == 8)
            frame_rate = *mocap_frate_raw.data<double>();
        else if (mocap_frate_raw.word_size == 4)
            frame_rate = *mocap_frate_raw.data<float>();
    } else {
        // Reasonable default
        std::cerr << "WARNING: mocap_framerate not present in '" << path.generic_string()
                  << "', assuming 120 FPS\n";
        frame_rate = 120.f;
    }
    return true;
}

// Instantiation
template class Sequence<sequence_config::AMASS>;