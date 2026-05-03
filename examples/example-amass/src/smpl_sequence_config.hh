#pragma once
#include <cstddef>

namespace sequence_config {

    namespace detail {
        template<class Derived>
        struct SequenceConfigBase {
            static constexpr size_t n_pose_params() {
                return (Derived::n_body_joints() + Derived::n_hand_joints() * 2) * 3;
            }
            // 0 to disable
            static constexpr size_t n_dmpls() { return 0; }
        };
    }  // namespace detail

    struct AMASS : public detail::SequenceConfigBase<AMASS> {
        static constexpr size_t n_shape_params() { return 16; }
        static constexpr size_t n_body_joints() { return 22; }
        static constexpr size_t n_hand_joints() { return 15; }
        static constexpr size_t n_dmpls() { return 8; }
    };

}  // namespace sequence_config