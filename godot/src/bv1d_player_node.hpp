#ifndef BV1D_PLAYER_NODE_HPP
#define BV1D_PLAYER_NODE_HPP

#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/sprite2d.hpp>

#include "bv1d_decoder_core.hpp"

namespace godot {

class BV1DPlayerNode : public Sprite2D {
    GDCLASS(BV1DPlayerNode, Sprite2D)

public:
    BV1DPlayerNode();
    ~BV1DPlayerNode();

    void set_file_path(const String& path);
    String get_file_path() const;

    void set_autoplay(bool enabled);
    bool get_autoplay() const;

    void set_loop(bool enabled);
    bool get_loop() const;

    void play();
    void pause();
    void stop();
    void seek(int frame);

    bool is_playing() const;
    int get_current_frame() const;
    int get_frame_count() const;
    double get_video_fps() const;

    void _ready() override;
    void _process(double delta) override;

protected:
    static void _bind_methods();

private:
    void open_video();
    void update_texture(const std::vector<uint8_t>& pixels);

    String file_path;
    bool autoplay = false;
    bool loop = false;
    bool playing = false;
    bool opened = false;
    double time_accumulator = 0.0;

    BV1DDecoderCore decoder;
    Ref<ImageTexture> texture;
};

}  // namespace godot

#endif
