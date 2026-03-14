#include "bv1d_player_node.hpp"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

namespace godot {

BV1DPlayerNode::BV1DPlayerNode() {}

BV1DPlayerNode::~BV1DPlayerNode() {
    if (opened) {
        decoder.close();
    }
}

void BV1DPlayerNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file_path", "path"), &BV1DPlayerNode::set_file_path);
    ClassDB::bind_method(D_METHOD("get_file_path"), &BV1DPlayerNode::get_file_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_path", PROPERTY_HINT_FILE, "*.bv1d"),
                 "set_file_path", "get_file_path");

    ClassDB::bind_method(D_METHOD("set_autoplay", "enabled"), &BV1DPlayerNode::set_autoplay);
    ClassDB::bind_method(D_METHOD("get_autoplay"), &BV1DPlayerNode::get_autoplay);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "get_autoplay");

    ClassDB::bind_method(D_METHOD("set_loop", "enabled"), &BV1DPlayerNode::set_loop);
    ClassDB::bind_method(D_METHOD("get_loop"), &BV1DPlayerNode::get_loop);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");

    ClassDB::bind_method(D_METHOD("play"), &BV1DPlayerNode::play);
    ClassDB::bind_method(D_METHOD("pause"), &BV1DPlayerNode::pause);
    ClassDB::bind_method(D_METHOD("stop"), &BV1DPlayerNode::stop);
    ClassDB::bind_method(D_METHOD("seek", "frame"), &BV1DPlayerNode::seek);

    ClassDB::bind_method(D_METHOD("is_playing"), &BV1DPlayerNode::is_playing);
    ClassDB::bind_method(D_METHOD("get_current_frame"), &BV1DPlayerNode::get_current_frame);
    ClassDB::bind_method(D_METHOD("get_frame_count"), &BV1DPlayerNode::get_frame_count);
    ClassDB::bind_method(D_METHOD("get_video_fps"), &BV1DPlayerNode::get_video_fps);

    ADD_SIGNAL(MethodInfo("playback_finished"));
}

void BV1DPlayerNode::set_file_path(const String& path) {
    file_path = path;
}

String BV1DPlayerNode::get_file_path() const {
    return file_path;
}

void BV1DPlayerNode::set_autoplay(bool enabled) {
    autoplay = enabled;
}

bool BV1DPlayerNode::get_autoplay() const {
    return autoplay;
}

void BV1DPlayerNode::set_loop(bool enabled) {
    loop = enabled;
}

bool BV1DPlayerNode::get_loop() const {
    return loop;
}

void BV1DPlayerNode::open_video() {
    if (opened) {
        decoder.close();
        opened = false;
    }

    if (file_path.is_empty()) {
        return;
    }

    String global_path = file_path;
    if (file_path.begins_with("res://")) {
        global_path = ProjectSettings::get_singleton()->globalize_path(file_path);
    }

    if (!decoder.open(global_path.utf8().get_data())) {
        UtilityFunctions::printerr("BV1DPlayerNode: Failed to open video: ",
                                   decoder.getLastError().c_str());
        opened = false;
        return;
    }

    opened = true;

    texture = ImageTexture::create_from_image(
        Image::create(decoder.getWidth(), decoder.getHeight(), false, Image::FORMAT_L8));
    set_texture(texture);

    // Display the first frame
    auto pixels = decoder.seekFrame(0);
    if (!pixels.empty()) {
        update_texture(pixels);
    }
}

void BV1DPlayerNode::update_texture(const std::vector<uint8_t>& pixels) {
    if (pixels.empty() || texture.is_null()) {
        return;
    }

    PackedByteArray pba;
    pba.resize(pixels.size());
    memcpy(pba.ptrw(), pixels.data(), pixels.size());

    Ref<Image> img = Image::create_from_data(
        decoder.getWidth(), decoder.getHeight(), false, Image::FORMAT_L8, pba);
    texture->update(img);
}

void BV1DPlayerNode::play() {
    if (!opened) {
        open_video();
    }
    if (opened) {
        playing = true;
        time_accumulator = 0.0;
    }
}

void BV1DPlayerNode::pause() {
    playing = false;
}

void BV1DPlayerNode::stop() {
    playing = false;
    time_accumulator = 0.0;
    if (opened) {
        auto pixels = decoder.seekFrame(0);
        if (!pixels.empty()) {
            update_texture(pixels);
        }
    }
}

void BV1DPlayerNode::seek(int frame) {
    if (!opened) {
        open_video();
    }
    if (opened) {
        auto pixels = decoder.seekFrame(frame);
        if (!pixels.empty()) {
            update_texture(pixels);
        }
    }
}

bool BV1DPlayerNode::is_playing() const {
    return playing;
}

int BV1DPlayerNode::get_current_frame() const {
    if (!opened) return 0;
    return decoder.getCurrentFrame();
}

int BV1DPlayerNode::get_frame_count() const {
    if (!opened) return 0;
    return decoder.getFrameCount();
}

double BV1DPlayerNode::get_video_fps() const {
    if (!opened) return 0.0;
    return decoder.getFPS();
}

void BV1DPlayerNode::_ready() {
    if (autoplay && !file_path.is_empty()) {
        play();
    } else if (!file_path.is_empty()) {
        open_video();
    }
}

void BV1DPlayerNode::_process(double delta) {
    if (!playing || !opened) {
        return;
    }

    double frame_duration = 1.0 / decoder.getFPS();
    time_accumulator += delta;

    while (time_accumulator >= frame_duration) {
        time_accumulator -= frame_duration;

        auto pixels = decoder.readFrame();
        if (pixels.empty()) {
            if (loop) {
                auto first = decoder.seekFrame(0);
                if (!first.empty()) {
                    update_texture(first);
                }
            } else {
                playing = false;
                emit_signal("playback_finished");
            }
            return;
        }

        update_texture(pixels);
    }
}

}  // namespace godot
