#include "smplx/smplx.hh"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "smplx/util.hh"
#include "smplx/util_cbor.hh"
#include "smplx/version.hh"

namespace smplx
{
    namespace {
        using util::assert_shape;
    }  // namespace

    template <class ModelConfig>
    std::shared_ptr<Model<ModelConfig>> Model<ModelConfig>::load(
        const std::filesystem::path& models_dir_path,
        const Gender model_gender
    ) {
        const auto model_file_path = util::find_model_file<ModelConfig>(
            models_dir_path,
            model_gender
        );

        if (!model_file_path.has_value()) {
            return nullptr; //throw std::runtime_error{ "SMPL model file not found" };
        }

        const auto model_uv_file_path = util::find_model_uv_file<ModelConfig>(
            models_dir_path
        );

        return std::make_shared<Model>(
            model_file_path.value(),
            model_gender,
            model_uv_file_path.value_or("") // optional
        );
    }

    template <class ModelConfig>
    Model<ModelConfig>::Model(
        const std::filesystem::path& model_path_,
        const Gender model_gender_,
        const std::filesystem::path& model_uv_path_/* optional */)
    {
        if (!std::ifstream(model_path_)) {
            throw std::runtime_error{ "ERROR: Failed to open model file" };
        }
        gender = model_gender_;
        auto cbor = util::cbor_object::load(model_path_);

        // Load kintree
        children.resize(n_joints());
        for (size_t i = 1; i < n_joints(); ++i) {
            children[ModelConfig::parent[i]].push_back(i);
        }

        // Load base template
        verts.noalias() = util::as_float_matrix(cbor.at("v_template").as_nparray(), n_verts(), 3);
        verts_load.noalias() = verts;

        // Load triangle mesh
        faces = util::as_uint_matrix(cbor.at("f").as_nparray(), n_faces(), 3);

        // Load joint regressor
        joint_reg.resize(n_joints(), n_verts());
        auto jreg_obj = cbor.at("J_regressor");
        auto jreg_sp_obj = jreg_obj.as_sparse_object();
        if (jreg_sp_obj.is_sparse_matrix()) {
            joint_reg = jreg_sp_obj.as_sparse_float_matrix(n_joints(), n_verts());
        } else {
            joint_reg = util::as_float_matrix(jreg_obj.as_nparray(), n_joints(), n_verts()).sparseView();
        }
        joints = joint_reg * verts;
        joint_reg.makeCompressed();

        // Load LBS weights
        weights.resize(n_verts(), n_joints());
        auto wt_obj = cbor.at("weights");
        auto wt_sp_obj = wt_obj.as_sparse_object();
        if (wt_sp_obj.is_sparse_matrix()) {
            weights = wt_sp_obj.as_sparse_float_matrix(n_verts(), n_joints());
        } else {
            weights = util::as_float_matrix(wt_obj.as_nparray(), n_verts(), n_joints()).sparseView();
        }
        weights.makeCompressed();

        blend_shapes.resize(3 * n_verts(), n_blend_shapes());
        // Load shape-dep blend shapes
        blend_shapes.template leftCols<n_shape_blends()>().noalias() =
            util::as_float_matrix(cbor.at("shapedirs").as_nparray(), 3 * n_verts(), n_shape_blends());

        // Load pose-dep blend shapes
        blend_shapes.template rightCols<n_pose_blends()>().noalias() =
            util::as_float_matrix(cbor.at("posedirs").as_nparray(), 3 * n_verts(), n_pose_blends());

        // Model has hand PCA (e.g. SMPLXpca), load hand PCA
        if constexpr (n_hand_pca() > 0) {
            if (cbor.contains("hands_meanl") && cbor.contains("hands_meanr")) {
                auto hml_obj = cbor.at("hands_meanl").as_nparray();
                auto hmr_obj = cbor.at("hands_meanr").as_nparray();
                auto hcl_obj = cbor.at("hands_componentsl").as_nparray();
                auto hcr_obj = cbor.at("hands_componentsr").as_nparray();

                auto hml_shape = hml_obj.get_shape();
                auto hmr_shape = hmr_obj.get_shape();

                util::assert_shape(hml_shape, {util::ANY_SHAPE});
                // Just verify basic consistency
                if (hml_shape.size() != 1 || hmr_shape.size() != 1 || hml_shape[0] != hmr_shape[0]) {
                     std::cerr << "ERROR: Hand mean shape mismatch\n";
                }

                size_t n_hand_params = hml_shape[0];
                _SMPLX_ASSERT_EQ(n_hand_params, n_hand_pca_joints() * 3);

                hand_mean_l = util::as_float_matrix(hml_obj, n_hand_params, 1);
                hand_mean_r = util::as_float_matrix(hmr_obj, n_hand_params, 1);

                hand_comps_l =
                    util::as_float_matrix(hcl_obj, n_hand_params, n_hand_params)
                        .topRows(n_hand_pca())
                        .transpose();
                hand_comps_r =
                    util::as_float_matrix(hcr_obj, n_hand_params, n_hand_params)
                        .topRows(n_hand_pca())
                        .transpose();
            }
        }

        // Maybe load UV (UV mapping WIP)
        if (!model_uv_path_.empty()) {
            std::ifstream ifs(model_uv_path_);
            ifs >> _n_uv_verts;
            if (_n_uv_verts) {
                if (ifs) {
                    // _SMPLX_ASSERT_LE(n_verts(), _n_uv_verts);
                    // Load the uv data
                    uv.resize(_n_uv_verts, 2);
                    for (size_t i = 0; i < _n_uv_verts; ++i)
                        ifs >> uv(i, 0) >> uv(i, 1);
                    _SMPLX_ASSERT(ifs);
                    uv_faces.resize(n_faces(), 3);
                    for (size_t i = 0; i < n_faces(); ++i) {
                        _SMPLX_ASSERT(ifs);
                        for (size_t j = 0; j < 3; ++j) {
                            ifs >> uv_faces(i, j);
                            // Make indices 0-based
                            --uv_faces(i, j);
                            _SMPLX_ASSERT_LT(uv_faces(i, j), _n_uv_verts);
                        }
                    }
                }
            }
        }
    #ifdef SMPLXPP_WITH_CUDA
        _cuda_load();
    #endif
    }

    template <class ModelConfig>
    Model<ModelConfig>::~Model() {
    #ifdef SMPLXPP_WITH_CUDA
        _cuda_free();
    #endif
    }

    template <class ModelConfig>
    void Model<ModelConfig>::set_deformations(const Eigen::Ref<const Points>& d) {
        verts.noalias() = verts_load + d;
    #ifdef SMPLXPP_WITH_CUDA
        _cuda_copy_template();
    #endif
    }

    template <class ModelConfig>
    void Model<ModelConfig>::set_template(const Eigen::Ref<const Points>& t) {
        verts.noalias() = t;
    #ifdef SMPLXPP_WITH_CUDA
        _cuda_copy_template();
    #endif
    }

    // Explicit Instantiations
    // https://learn.microsoft.com/en-us/cpp/cpp/explicit-instantiation?view=msvc-170
    template class Model<model_config::SMPL>;
    template class Model<model_config::SMPL_v1>;
    template class Model<model_config::SMPLH>;
    template class Model<model_config::SMPLX>;
    template class Model<model_config::SMPLXpca>;
    template class Model<model_config::SMPLX_v1>;
    template class Model<model_config::SMPLXpca_v1>;

}  // namespace smplx
