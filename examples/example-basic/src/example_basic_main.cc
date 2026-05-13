#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include <triengine/visualization/visualizer.hh>
#include <triengine/gui/windows/scene_ctrl_window.hh>

#include <smplx/smplx.hh>
#include <smplx/util.hh>

#include <memory>
#include <limits>

class basic_control_window
    : public triengine::gui::iwindow
{
public:
    using smpl_model_type = smplx::model_config::SMPL; // smplx::model_config::SMPLX;

    struct ui_state_t
    {
        bool flag_force_cpu{ true };
        bool flag_pose_blends{ false };
        bool flag_update_needed{ false };

        triengine::color3_f32 smpl_mesh_color{ 1.0f, 1.0f, 1.0f };
        triengine::geometry::mesh_object::vertex_shading_material smpl_mesh_material{
            /*ambient_intensity_*/0.5f,
            /*diffuse_intensity_*/0.9f,
            /*specular_intensity_*/0.4f,
            /*shininess_*/128,
            /*alpha_*/1.0f
        };

        ui_state_t() = default;
    };

private:
    std::shared_ptr<triengine::scene> _scene;
    std::filesystem::path _smpl_models_dir;
    std::shared_ptr<smplx::Model<smpl_model_type>> _smpl_model;
    std::unique_ptr<smplx::Body<smpl_model_type>> _curr_smpl_body;
    ui_state_t _ui_state;

    std::shared_ptr<triengine::geometry::mesh_object> _smpl_mesh;

public:
    basic_control_window(
        std::shared_ptr<triengine::scene> scene,
        const std::filesystem::path& smpl_models_dir)
        : _scene{ std::move(scene) }
        , _smpl_models_dir{ smpl_models_dir }
    {
        // Construct SMPL model & body with default gender
        _smpl_model = smplx::Model<smpl_model_type>::load(
            _smpl_models_dir,
            smplx::Gender::female
        );

        if (!_smpl_model) {
            throw std::runtime_error{ "Failed to load SMPL model" };
        }

        _curr_smpl_body = std::make_unique<smplx::Body<smpl_model_type>>(*_smpl_model);

        _curr_smpl_body->trans() <<
            0.0f, 0.0f, 0.0f;

        Eigen::RowVector<float, 10> shape_row;
        shape_row <<
             0.4721353f,   0.6002891f,  -0.09081578f,  0.08812723f, -0.08782649f, 
            -0.08549979f, -0.08885491f,  0.11558536f,  0.01512934f, -0.02311003f;

        _curr_smpl_body->shape().segment<10>(0) = shape_row.transpose();

        _curr_smpl_body->pose() <<
            -2.117343f,    1.4077201f,   0.7652855f,   0.35792455f,  0.4519118f,   1.0953735f,
            -0.9581131f,  -0.03678823f, -0.7684142f,   0.25126573f, -0.13709834f, -0.05213431f,
            1.2294062f,  -0.8674625f,   0.33215177f, -0.02145357f,  0.43281138f, -0.26208255f,
            -0.2543664f,   0.10186865f, -0.28364652f, -0.01746661f,  0.13900025f, -0.5437213f,
            0.00403607f, -0.25628126f,  0.42785037f, -0.12414886f,  0.01769107f, -0.18041986f,
            -0.43790978f,  0.04023361f,  0.54148155f, -0.10405298f, -0.03536972f, -0.75982624f,
            -0.4643529f,  -0.3340001f,  -0.11342303f, -0.03522978f,  0.3506581f,  -0.13024633f,
            -0.19007441f, -0.08522718f, -0.23865803f, -0.23099706f, -0.16799903f,  0.1077709f,
            -0.0477278f,   0.273731f,   -0.31532052f, -0.30561548f,  0.07433914f, -0.25790593f,
            -0.3594417f,  -0.402572f,    0.197921f,   -0.27091503f,  0.6389861f,  -0.12178899f,
            -0.18070883f, -0.05442453f, -0.05552595f,  0.07416078f,  0.11782224f, -0.04099467f,
            -0.0978334f,   0.01885751f, -0.06627318f, -0.06842318f,  0.03484467f,  0.13122012f;

        _ui_state.flag_update_needed = true;
    }


    virtual ~basic_control_window() {}

    const ui_state_t* get_ui_state() const noexcept {
        return &_ui_state;
    }

    const char* get_window_name() const override {
        return "Basic Model Control";
    }

    ImVec2 get_initial_window_size() const override {
        return ImVec2{ 430.0f, 470.0f };
    }

    void render(
        [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
    {
        if (_ui_state.flag_update_needed)
        {
            _ui_state.flag_update_needed = false;
            _curr_smpl_body->update();
            this->_update_scene();
        }

        ImGui::Text("Model Type: %s (%s)"
            , _smpl_model->name()
            , smplx::util::gender_to_str(_smpl_model->gender)
        );

        ImGui::TextUnformatted("Reset: "); {
            ImGui::SameLine();
            if (ImGui::Button("Trans##ResetTrans")) {
                _curr_smpl_body->trans().setZero();
                _ui_state.flag_update_needed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Pose##ResetPose")) {
                _curr_smpl_body->pose().setZero();
                _ui_state.flag_update_needed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Hand##ResetHand")) {
                _curr_smpl_body->hand_pca().setZero();
                _ui_state.flag_update_needed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Shape##ResetShape")) {
                _curr_smpl_body->shape().setZero();
                _ui_state.flag_update_needed = true;
            }
        }

        if (ImGui::Button("Export Mesh"))
        {
            const auto& verts = _curr_smpl_body->verts();
            const auto& faces = _smpl_model->faces;

            TRIENGINE_TRACE("Writing OBJ files ...");

            {
                std::ofstream out{ "smpl_body_export.obj" };

                for (int64_t i = 0; i < verts.rows(); ++i) {
                    out << "v "
                        << verts(i, 0) << ' '
                        << verts(i, 1) << ' '
                        << verts(i, 2) << '\n';
                }

                for (int64_t i = 0; i < faces.rows(); ++i) {
                    out << "f "
                        << faces(i, 0) + 1 << ' '
                        << faces(i, 1) + 1 << ' '
                        << faces(i, 2) + 1 << '\n';
                }

                out.close();
            }

            TRIENGINE_TRACE("All done.");
        }

        if (ImGui::CollapsingHeader("Pose", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            const int STEP = 10;
            for (size_t j = 0; j < _smpl_model->n_explicit_joints(); j += STEP)
            {
                size_t end_idx = std::min(j + STEP, _smpl_model->n_explicit_joints());
                if (ImGui::TreeNode(fmt::format("Angle axis {}-{}", j, end_idx - 1).c_str())) {
                    for (size_t i = j; i < end_idx; ++i) {
                        if (ImGui::DragFloat3(
                            fmt::format("{}##joint{}", _smpl_model->joint_name(i), i).c_str(),
                            _curr_smpl_body->pose().data() + i * 3,
                            0.01f,
                            -1.6f, 
                            1.6f))
                        {
                            _ui_state.flag_update_needed = true;
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Shape"))
        {
            ImGui::Indent();
            for (size_t i = 0; i < _smpl_model->n_shape_blends(); ++i) {
                if (ImGui::DragFloat(
                    fmt::format("shape{}", i).c_str(),
                    _curr_smpl_body->shape().data() + i, 
                    0.01f,
                    -5.0f, 
                    5.0f))
                {
                    _ui_state.flag_update_needed = true;
                }
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Mesh Material"))
        {
            ImGui::Indent();
            if (ImGui::ColorEdit3("Mesh Color##SMPLMeshMat", _ui_state.smpl_mesh_color.data())) {
                _ui_state.flag_update_needed = true;
            }
            if (ImGui::DragFloat("Ambient Intensity##SMPLMeshMat", &_ui_state.smpl_mesh_material.ambient_intensity, 0.001f, 0.0f, 10.0f)) {
                _ui_state.flag_update_needed = true;
            }
            if (ImGui::DragFloat("Diffuse Intensity##SMPLMeshMat", &_ui_state.smpl_mesh_material.diffuse_intensity, 0.001f, 0.0f, 10.0f)) {
                _ui_state.flag_update_needed = true;
            }
            if (ImGui::DragFloat("Specular Intensity##SMPLMeshMat", &_ui_state.smpl_mesh_material.specular_intensity, 0.001f, 0.0f, 10.0f)) {
                _ui_state.flag_update_needed = true;
            }
            if (ImGui::DragFloat("Alpha##SMPLMeshMat", &_ui_state.smpl_mesh_material.alpha, 0.01f, 0.0f, 1.0f)) {
                _ui_state.flag_update_needed = true;
            }
            ImGui::Unindent();
        }
    }

private:
    void _update_scene()
    {
        bool newly_created = false;
        if (!_smpl_mesh) {
            _smpl_mesh = std::make_shared<triengine::geometry::mesh_object>();
            newly_created = true;
        }

        float min_y_pos{ std::numeric_limits<float>::max() };
        float max_y_pos{ std::numeric_limits<float>::lowest() };
        {
            const smplx::Points& smpl_verts = _curr_smpl_body->verts();
            //CXLIB_TRACE("src_verts: {} ({}x{})", _smpl_model->n_verts(), smpl_verts.rows(), smpl_verts.cols());

            auto& verts = _smpl_mesh->vertex_positions;
            verts.clear();
            verts.reserve(smpl_verts.rows());
            for (int64_t i = 0; i < smpl_verts.rows(); ++i) {
                const auto& v = smpl_verts.row(i);
                verts.emplace_back(v.x(), v.y(), v.z());
                min_y_pos = std::min(min_y_pos, v.y());
                max_y_pos = std::max(max_y_pos, v.y());
            }
        }
        //CXLIB_TRACE("model_height: {} (min_y: {}, max_y: {})", max_y_pos - min_y_pos, min_y_pos, max_y_pos);

        {
            const smplx::Triangles& smpl_faces = _smpl_model->faces;
            //CXLIB_TRACE("src_faces: {} ({}x{})", _smpl_model->n_faces(), smpl_faces.rows(), smpl_faces.cols());

            auto& faces = _smpl_mesh->triangle_indices;
            faces.clear();
            faces.reserve(smpl_faces.rows());
            for (int64_t i = 0; i < smpl_faces.rows(); ++i) {
                const auto& f = smpl_faces.row(i);
                faces.emplace_back(f.x(), f.y(), f.z());
            }
        }

        _smpl_mesh->compute_vertex_normals(true);
        _smpl_mesh->paint_uniform_color(triengine::color3_f32::all(1.0f));

        Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
        R = Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
            * Eigen::AngleAxisf(triengine::math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
            * Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitX());
        _smpl_mesh->rotate(R, false);

        _smpl_mesh->translate(triengine::vec3_f32(0.0f, -min_y_pos, 1.0f), false);

        *_smpl_mesh->get_vertex_shading_material() = _ui_state.smpl_mesh_material;

        if (newly_created) {
            _scene->add_geometry(_smpl_mesh);
        } else {
            _smpl_mesh->mark_dirty();
        }
    }

}; // class


class basic_viewer_app
{
private:
    std::unique_ptr<triengine::visualization::visualizer> _vis;
    std::shared_ptr<triengine::scene> _scene;

    std::shared_ptr<triengine::gui::log_window> _log_window;
    std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;
    std::shared_ptr<triengine::gui::scene_control_window> _scene_ctrl_window;
    std::shared_ptr<basic_control_window> _smpl_window;

    std::shared_ptr<triengine::geometry::mesh_object> _smpl_mesh;

public:
    basic_viewer_app() = default;
    ~basic_viewer_app() = default;

    void create(
        const std::filesystem::path& smpl_models_dir)
    {
        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("Basic SMPL Viewer"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")"
        );

        _scene = _vis->add_scene();
        _scene->set_name("main");
        _scene->get_render_config()->bg_color = triengine::color4_f32{ 0.03f, 0.03f, 0.03f, 1.0f };
        _scene->get_render_config()->pcd_point_size = 2.5f;
        _scene->get_render_config()->enable_anti_aliasing = true;
        _scene->get_render_config()->light_opts.dir_light.enabled = true;
        _scene->get_render_config()->light_opts.dir_light.follow_camera = true;
        _scene->get_render_config()->light_opts.dir_light.ambient_intensity = 0.10f;
        _scene->get_render_config()->light_opts.dir_light.diffuse_intensity = 0.35f;
        _scene->get_render_config()->light_opts.dir_light.specular_intensity = 0.20f;
        _scene->get_render_config()->light_opts.point_light.enabled = true;
        _scene->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, 2.0f, 0.0f };
        _scene->get_render_config()->light_opts.point_light.light_source_color_intensity = 48.0f;
        _scene->get_render_config()->light_opts.point_light.show_light_source = false;
        _scene->get_render_config()->light_opts.point_light.ambient_intensity = 0.00f;
        _scene->get_render_config()->light_opts.point_light.diffuse_intensity = 1.00f;
        _scene->get_render_config()->light_opts.point_light.specular_intensity = 1.50f;
        _scene->get_render_config()->light_opts.simple_fog.fog_density = 0.060f;
        _scene->get_render_config()->light_opts.simple_fog.fog_start_dist = 6.50f;
        _scene->get_render_config()->light_opts.bloom.strength = 0.03f;
        _scene->get_render_config()->light_opts.hdr.exposure = 0.22f;
        _scene->get_render_config()->text_render_opts.dist_scale_opts.enabled = false;
        _scene->get_render_config()->text_render_opts.depth_test_opts.enabled = false;
        _scene->switch_camera_type(triengine::camera_type::arcball);
        _scene->get_camera()->as<triengine::arcball_camera>()->set_fovy(20.0f);

        {
            _scene->get_render_config()->show_origin_xz_grid = true;
            _scene->get_render_config()->inf_plane_opts.max_view_distance = 35.0f;
            _scene->get_render_config()->inf_plane_opts.grid_cell_size = 0.5f;

            triengine::box_filtered_chess_plane_option_t inf_plane_opt{};
            inf_plane_opt.grid_cell_color1 = triengine::vec3_f32{ 0.74f, 0.74f, 0.74f };
            inf_plane_opt.grid_cell_color2 = triengine::vec3_f32{ 0.90f, 0.90f, 0.90f };
            inf_plane_opt.material.ambient_intensity = 0.2f;
            inf_plane_opt.material.diffuse_intensity = 0.7f;
            inf_plane_opt.material.specular_intensity = 0.5f;
            _scene->get_render_config()->inf_plane_opts.plane_option = inf_plane_opt;
        }

        _vis->set_key_callback(
            [this](
                [[maybe_unused]] const int32_t key,
                [[maybe_unused]] const int32_t scancode,
                [[maybe_unused]] const int32_t action,
                [[maybe_unused]] const int32_t mods,
                [[maybe_unused]] bool& handled)
            {
                if (action != GLFW_RELEASE)
                {
                    switch (key) {
                    case GLFW_KEY_ESCAPE:
                        break;
                    case GLFW_KEY_F12:
                        _vis->enable_main_menu(!_vis->is_main_menu_enabled());
                        break;
                    }
                }
            });

        _scene->add_geometry(triengine::geometry::mesh_object::create_coordinate_frame(0.3f));

        // add render stats window
        _render_stats_window = std::make_shared<triengine::gui::render_stats_window>();
        _render_stats_window->set_visible(false);
        _vis->add_gui_window(_render_stats_window);

        // add log window
        _log_window = std::make_shared<triengine::gui::log_window>();
        _log_window->set_visible(false);
        _vis->add_gui_window(_log_window);

        _scene_ctrl_window = std::make_shared<triengine::gui::scene_control_window>(_vis.get());
        _vis->add_gui_window(_scene_ctrl_window, triengine::gui::dock_slot::left);

        _smpl_window = std::make_shared<basic_control_window>(_scene, smpl_models_dir);
        _vis->add_gui_window(_smpl_window, triengine::gui::dock_slot::left);
    }

    void destroy()
    {
        _log_window.reset();
        _render_stats_window.reset();

        _vis->destroy_window();
        _vis.reset();
    }

    void run()
    {
        CXLIB_INFO("polling start..");
        while (_vis->update_window())
        {
            _vis->render();
        } // while
    }

}; // class


int main(
    [[maybe_unused]] int argc, 
    [[maybe_unused]] char** argv)
{
    _CXLIB utils::logger::instance().init(_CXLIB utils::logger::init_options()
        .set_logger_name("example-basic")
        .enable_stdout_logging(_CXLIB utils::logger::level::trace, "| %n | %^--%L--%$ | TID %t | %s:%# | %v")
        .enable_async_mode()
    );

    int retval{ -1 };

    try
    {
        CXLIB_TRACE("Build: {}, {}", __DATE__, __TIME__);
        CXLIB_TRACE("SMPLXPP CUDA Support: {}", smplx::cuda_supported());

        const std::filesystem::path curr_image_dir_path = _CXLIB utils::get_current_module_image_path().parent_path();

        triengine::global_options::instance()->set_resource_directory(curr_image_dir_path / "../resources");
        triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::warn);
        triengine::global_options::instance()->get_logger().register_print_callback(
            [](triengine::utility::log_level lv, std::string_view msg_sv)
        {
            _CXLIB utils::logger::instance().log(
                [lv]() -> _CXLIB utils::logger::level {
                switch (lv) {
                case triengine::utility::log_level::trace: return _CXLIB utils::logger::level::trace;
                case triengine::utility::log_level::debug: return _CXLIB utils::logger::level::debug;
                case triengine::utility::log_level::info: return _CXLIB utils::logger::level::info;
                case triengine::utility::log_level::warn: return _CXLIB utils::logger::level::warn;
                case triengine::utility::log_level::error: return _CXLIB utils::logger::level::error;
                case triengine::utility::log_level::critical: return _CXLIB utils::logger::level::critical;
                default: return _CXLIB utils::logger::level::warn;
                }
            }(), msg_sv);
        });

        const auto models_dir = std::filesystem::canonical(curr_image_dir_path / "../../../../models");
        {
            basic_viewer_app app;

            CXLIB_INFO("Creating app..");
            app.create(models_dir);

            CXLIB_INFO("Running app..");
            app.run();

            CXLIB_INFO("Destroying app..");
            app.destroy();
        }

        CXLIB_INFO("all done!");
        retval = 0;
    }
    catch (const std::exception& e)
    {
        CXLIB_CRITICAL("Main Exception: {}", e.what());
    }
    catch (...)
    {
        CXLIB_CRITICAL("Main Unknown Exception!!");
    }

    _CXLIB utils::logger::instance().deinit();
    return retval;
}