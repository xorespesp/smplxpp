#pragma once
#include "smpl_sequence_config.hh"

#include <smplx/smplx.hh>

#include <filesystem>
#include <type_traits>

// An AMASS-compatible body pose+translation[+DMPL] sequence
// with overall shape and gender information
// SequenceConfig: pick from smplx::sequence_config::*
// (currently only AMASS available)
template <class SequenceConfig>
class Sequence {
public:
    // Create sequence and load from AMASS-like .npz, with fields:
    // trans, gender (optional), mocap_framerate (optional),
    // betas, dmpls, poses.
    // If path is empty, constructs empty sequence (n_frames = 0).
    explicit Sequence(const std::filesystem::path& path = {});

    // Load sequence from AMASS-like .npz, with fields:
    // trans, gender (optional), mocap_framerate (optional),
    // betas, dmpls, poses
    // Returns true on success
    bool load(const std::filesystem::path& path);

    // Set body shape
    template <class ModelConfig>
    void set_shape([[maybe_unused]] smplx::Body<ModelConfig>& body) {
        if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPL>) {
            body.shape().noalias() =
                shape.template head<smplx::model_config::SMPL::n_shape_blends()>();
        } else if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPLH>) {
            body.shape().noalias() = shape;
        } else if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPLX>
                          || std::is_same_v<ModelConfig, smplx::model_config::SMPLX_v1>) {
            // Shape space is not compatible, do nothing
        } else {
            static_assert(sizeof(ModelConfig) == 0,
                "Sequence does not currently support this model");
        }
    }

    // Set body pose and root transform for the given frame
    template <class ModelConfig>
    void set_pose(smplx::Body<ModelConfig>& body, size_t frame) {
        if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPL>) {
            constexpr size_t n_common = SequenceConfig::n_body_joints() * 3;
            body.trans().noalias() = trans.row(frame).transpose();
            body.pose().template head<n_common>().noalias() =
                pose.row(frame).template head<n_common>().transpose();
            // Remaining joints assumed to already be set to 0
        } else if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPLH>) {
            body.trans().noalias() = trans.row(frame).transpose();
            body.pose().noalias() = pose.row(frame).transpose();
        } else if constexpr (std::is_same_v<ModelConfig, smplx::model_config::SMPLX>
                          || std::is_same_v<ModelConfig, smplx::model_config::SMPLX_v1>) {
            constexpr size_t n_body_common = SequenceConfig::n_body_joints() * 3;
            constexpr size_t n_hand_common = SequenceConfig::n_hand_joints() * 6;
            body.trans().noalias() = trans.row(frame).transpose();
            body.pose().template head<n_body_common>().noalias() =
                pose.row(frame).template head<n_body_common>().transpose();
            body.pose().template tail<n_hand_common>().noalias() =
                pose.row(frame).template tail<n_hand_common>().transpose();
            // 3 middle face joints assumed to already be set to 0
        } else {
            static_assert(sizeof(ModelConfig) == 0,
                "Sequence does not currently support this model");
        }
    }

    // * METADATA
    // Number of frames in sequence
    size_t n_frames{};

    // Mocap frame rate
    double frame_rate{};

    // Gender, may be unknown
    smplx::Gender gender{ smplx::Gender::unknown };

    // * BODY DATA
    // Extended shape parameters (betas)
    Eigen::Matrix<smplx::Scalar, SequenceConfig::n_shape_params(), 1> shape;

    // Root translations
    Eigen::Matrix<smplx::Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> trans;

    // Pose parameters
    Eigen::Matrix<smplx::Scalar, Eigen::Dynamic, SequenceConfig::n_pose_params(),
                  Eigen::RowMajor>
        pose;

    // DMPLs
    Eigen::Matrix<smplx::Scalar, Eigen::Dynamic, SequenceConfig::n_dmpls(),
                  Eigen::RowMajor>
        dmpls;
};
