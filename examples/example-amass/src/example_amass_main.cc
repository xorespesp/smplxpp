#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include <triengine/visualization/visualizer.hh>
#include <triengine/gui/windows/scene_ctrl_window.hh>
#include <triengine/gui/widgets/file_browser_widget.hh>

#include <smplx/smplx.hh>
#include <smplx/util.hh>
#include "smpl_sequence.hh"

#include <filesystem>
#include <memory>
#include <limits>
#include <chrono>

class amass_playback_control_window
    : public triengine::gui::iwindow
{
public:
    using smpl_model_type = smplx::model_config::SMPLX;
    using smpl_sequence_type = Sequence<sequence_config::AMASS>;

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

        // playback animation state
        bool is_playing{ false };
        bool camera_follow_human{ false };
        int32_t curr_frame_idx{ 0 };
        
        // When play() is called, these capture the starting point
        int32_t playback_start_frame{ 0 };  // Frame index when playback started
        std::chrono::high_resolution_clock::time_point playback_start_time{};  // Wall time when playback started

        ui_state_t() = default;
    };

private:
    std::shared_ptr<triengine::scene> _scene;
    std::filesystem::path _smpl_models_dir;
    std::shared_ptr<smplx::Model<smpl_model_type>> _smpl_model;
    std::unique_ptr<smplx::Body<smpl_model_type>> _curr_smpl_body;
    std::unique_ptr<smpl_sequence_type> _loaded_amass_seq;
    std::filesystem::path _loaded_amass_seq_path;
    ui_state_t _ui_state;
    triengine::gui::widgets::file_browser_widget _file_browser;

    std::shared_ptr<triengine::geometry::mesh_object> _smpl_mesh;

public:
    amass_playback_control_window(
        std::shared_ptr<triengine::scene> scene,
        const std::filesystem::path& smpl_models_dir)
        : _scene{ std::move(scene) }
        , _smpl_models_dir{ smpl_models_dir }
    {
        // Construct SMPL model & body with default gender
        _smpl_model = smplx::Model<smpl_model_type>::load(
            _smpl_models_dir,
            smplx::Gender::male
        );

        if (!_smpl_model) {
            throw std::runtime_error{ "Failed to load SMPL model" };
        }

        _curr_smpl_body = std::make_unique<smplx::Body<smpl_model_type>>(*_smpl_model);

        const std::filesystem::path curr_image_dir_path = _CXLIB utils::get_current_module_image_path().parent_path();
        _file_browser.register_file_filters({ "*.npz", "*.*" });
        _file_browser.set_active_file_filter(0);
        _file_browser.set_cwd(curr_image_dir_path / "../../../../examples/example-amass/samples"); // root project directory
    }

    virtual ~amass_playback_control_window() {}

    bool is_loaded() const noexcept {
        return (_loaded_amass_seq && _loaded_amass_seq->n_frames > 0);
    }

    bool is_playing() const noexcept {
        return this->is_loaded() && _ui_state.is_playing;
    }

    bool is_paused() const noexcept {
        return this->is_loaded() && !_ui_state.is_playing;
    }

    void load_amass_sequence(const std::filesystem::path& seq_file_path)
    {
        try {
            _loaded_amass_seq = std::make_unique<smpl_sequence_type>(seq_file_path);
            _loaded_amass_seq_path = seq_file_path;

            if (_loaded_amass_seq->n_frames > 0) {
                // Load shape and pose from AMASS
                _loaded_amass_seq->set_shape(*_curr_smpl_body);
                _loaded_amass_seq->set_pose(*_curr_smpl_body, 0);

                // Check if gender changed
                if (_loaded_amass_seq->gender != _smpl_model->gender) {
                    _smpl_model = smplx::Model<smpl_model_type>::load(
                        _smpl_models_dir,
                        _loaded_amass_seq->gender
                    );

                    if (!_smpl_model) {
                        throw std::runtime_error{ "Failed to load SMPL model" };
                    }

                    _curr_smpl_body = std::make_unique<smplx::Body<smpl_model_type>>(*_smpl_model);
                }

                _ui_state.curr_frame_idx = 0;
                _ui_state.playback_start_frame = 0;
                _ui_state.is_playing = false;
                _ui_state.flag_update_needed = true;

                CXLIB_INFO("Loaded AMASS sequence: {} ({} frames, {} fps)"
                    , seq_file_path.generic_string()
                    , _loaded_amass_seq->n_frames
                    , _loaded_amass_seq->frame_rate
                );
            }
        }
        catch (const std::exception& e) {
            CXLIB_ERROR("Failed to load AMASS sequence: {}", e.what());
            _loaded_amass_seq.reset();
            _loaded_amass_seq_path.clear();
        }
    }

    void close()
    {
        _loaded_amass_seq.reset();
        _loaded_amass_seq_path.clear();
    }

    void update_amass_frame(int32_t new_frame_idx)
    {
        if (!_loaded_amass_seq || _loaded_amass_seq->n_frames <= 0) { return; }
        new_frame_idx = std::clamp(new_frame_idx, 0, static_cast<int32_t>(_loaded_amass_seq->n_frames) - 1);
        if (new_frame_idx != _ui_state.curr_frame_idx) { // Only update if frame actually changed
            _loaded_amass_seq->set_pose(*_curr_smpl_body, static_cast<size_t>(new_frame_idx));
            _ui_state.curr_frame_idx = new_frame_idx;
            _ui_state.flag_update_needed = true;
        }
    }

    void previous_frame()
    {
        if (!this->is_loaded()) { return; }
        _ui_state.is_playing = false;
        this->update_amass_frame(_ui_state.curr_frame_idx - 1);
    }

    void next_frame()
    {
        if (!this->is_loaded()) { return; }
        _ui_state.is_playing = false;
        this->update_amass_frame(_ui_state.curr_frame_idx + 1);
    }

    void play()
    {
        if (this->is_loaded()) {
            _ui_state.is_playing = true;
            _ui_state.playback_start_frame = _ui_state.curr_frame_idx;
            _ui_state.playback_start_time = std::chrono::high_resolution_clock::now();
        }
    }

    void pause()
    {
        if (this->is_loaded()) {
            _ui_state.is_playing = false;
        }
    }

    const ui_state_t* get_ui_state() const noexcept {
        return &_ui_state;
    }

    const char* get_window_name() const override {
        return "AMASS Playback Control";
    }

    ImVec2 get_initial_window_size() const override {
        return ImVec2{ 430.0f, 650.0f };
    }

    void render(
        [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
    {
        // Update animation - calculate frame based on elapsed time
        if (this->is_loaded() && _ui_state.is_playing)
        {
            const double elapsed = std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now() - _ui_state.playback_start_time
            ).count();
            
            // target_frame_idx = playback_start_frame_idx + (elapsed_time * playback_fps)
            const int32_t target_frame_idx = 
                _ui_state.playback_start_frame + 
                static_cast<int32_t>(std::floor(elapsed * _loaded_amass_seq->frame_rate));
            
            if (target_frame_idx < static_cast<int32_t>(_loaded_amass_seq->n_frames)) {
                this->update_amass_frame(target_frame_idx);
            } else {
                // End of sequence - stop and reset
                _ui_state.is_playing = false;
                this->update_amass_frame(0);
            }
        }

        if (_ui_state.flag_update_needed)
        {
            _ui_state.flag_update_needed = false;
            _curr_smpl_body->update();
            this->_update_scene();
        }

        if (this->is_loaded())
        {
            ImGui::Text("Model Type: %s (%s)"
                , _smpl_model->name()
                , smplx::util::gender_to_str(_smpl_model->gender)
            );

            const int32_t total_frames = static_cast<int32_t>(_loaded_amass_seq->n_frames);

            ImGui::TextWrapped("File: %s", _loaded_amass_seq_path.filename().string().c_str());
            ImGui::Text("Playback Speed: %.2f fps", _loaded_amass_seq->frame_rate);

            if (ImGui::Button("<<")) {
                _ui_state.is_playing = false;
                this->update_amass_frame(0);
            }

            ImGui::SameLine();
            if (ImGui::Button(_ui_state.is_playing ? "||" : ">")) {
                if (_ui_state.is_playing) {
                    this->pause();
                } else {
                    this->play();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button(">>")) {
                _ui_state.is_playing = false;
                this->update_amass_frame(total_frames - 1);
            }

            ImGui::SameLine();
            if (ImGui::Button("close")) {
                this->close();
                return;
            }

            // User can drag slider to any frame
            if (ImGui::SliderInt("##FrameSlider", &_ui_state.curr_frame_idx, 0, total_frames - 1)) {
                _ui_state.is_playing = false;
                this->update_amass_frame(_ui_state.curr_frame_idx);
            }
            ImGui::SameLine();
            ImGui::Text("/ %d frames", total_frames);

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

            ImGui::Checkbox("Camera Follow Human", &_ui_state.camera_follow_human);
        }
        else //if (!this->is_loaded())
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Select AMASS sequence file (SMPL-X G) to play...");

            if (_file_browser.show())
            {
                this->load_amass_sequence(_file_browser.get_selected_path());
            }
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
            * Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitY())
            * Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());
        _smpl_mesh->rotate(R, false);

        //_smpl_mesh->translate(triengine::vec3_f32(0.0f, -min_y_pos, 1.0f), false);

        *_smpl_mesh->get_vertex_shading_material() = _ui_state.smpl_mesh_material;

        if (newly_created) {
            _scene->add_geometry(_smpl_mesh);
        } else {
            _smpl_mesh->mark_dirty();
        }

        if (_ui_state.camera_follow_human) {
            // Set camera's center of rotation to transformed root joint
            auto center_of_rot = (
                _smpl_mesh->get_model()/* model matrix */ *
                _curr_smpl_body->joints()/* deformed root joint */ 
                .row(0)
                .transpose()
                .homogeneous()
            ).template head<3>();
            auto cam = dynamic_cast<triengine::arcball_camera*>(_scene->get_camera());
            cam->set_pivot_point(center_of_rot);
        }
    }

}; // class


class amass_viewer_app
{
private:
    std::unique_ptr<triengine::visualization::visualizer> _vis;
    std::shared_ptr<triengine::scene> _scene;

    std::shared_ptr<triengine::gui::log_window> _log_window;
    std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;
    std::shared_ptr<triengine::gui::scene_control_window> _scene_ctrl_window;
    std::shared_ptr<amass_playback_control_window> _amass_playback_window;

public:
    amass_viewer_app() = default;
    ~amass_viewer_app() = default;

    void create(
        const std::filesystem::path& smpl_models_dir)
    {
        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("AMASS Sequence Viewer"
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
                case GLFW_KEY_SPACE:
                    if (_amass_playback_window && _amass_playback_window->is_loaded()) {
                        if (_amass_playback_window->is_playing()) {
                            _amass_playback_window->pause();
                        } else {
                            _amass_playback_window->play();
                        }
                    }
                    break;
                case GLFW_KEY_RIGHT:
                    if (_amass_playback_window && _amass_playback_window->is_loaded()) {
                        _amass_playback_window->next_frame();
                    }
                    break;
                case GLFW_KEY_LEFT:
                    if (_amass_playback_window && _amass_playback_window->is_loaded()) {
                        _amass_playback_window->previous_frame();
                    }
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

        _amass_playback_window = std::make_shared<amass_playback_control_window>(_scene, smpl_models_dir);
        _vis->add_gui_window(_amass_playback_window, triengine::gui::dock_slot::left);
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
        .set_logger_name("example-amass")
        .enable_stdout_logging(_CXLIB utils::logger::level::trace, "| %n | %^--%L--%$ | TID %t | %s:%# | %v")
        .enable_async_mode()
    );

    int retval{ -1 };

    try
    {
        CXLIB_TRACE("Build: {}, {}", __DATE__, __TIME__);

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

        const std::filesystem::path models_dir_path = 
            std::filesystem::canonical(curr_image_dir_path / "../../../../models");

        {
            amass_viewer_app app;

            CXLIB_INFO("Creating app..");
            app.create(models_dir_path);

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