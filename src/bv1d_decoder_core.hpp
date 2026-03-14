#ifndef BV1D_DECODER_CORE_HPP
#define BV1D_DECODER_CORE_HPP

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

class BV1DDecoderCore {
public:
    // Returns true on success. On failure, sets last_error.
    bool open(const std::string& path);
    std::vector<uint8_t> readFrame();
    std::vector<uint8_t> seekFrame(int index);
    void close();

    int getWidth() const;
    int getHeight() const;
    double getFPS() const;
    int getFrameCount() const;
    int getCurrentFrame() const;
    const std::string& getLastError() const;

protected:
    struct FrameIndexEntry {
        uint64_t offset;
        uint32_t size;
    };

    struct Header {
        uint8_t version;
        uint16_t width;
        uint16_t height;
        uint16_t fps_num;
        uint16_t fps_den;
        uint32_t frame_count;
        uint16_t keyframe_interval;
        uint8_t flags;
        uint64_t index_offset;
    };

    std::ifstream file;
    Header header{};
    std::vector<FrameIndexEntry> frame_index;
    std::vector<uint8_t> prev_packed;
    int current_frame = 0;
    int last_decoded_frame = -1;
    std::string last_error;

private:
    bool readHeader();
    bool readFrameIndex();
    std::vector<uint8_t> readRawFrame(int index);
    std::vector<uint8_t> huffmanDecode(const uint8_t* data, size_t size, uint32_t unpacked_size);
    std::vector<uint8_t> unpackBits(const std::vector<uint8_t>& packed);
    std::vector<uint8_t> decodeFrameAt(int index);
};

#endif
