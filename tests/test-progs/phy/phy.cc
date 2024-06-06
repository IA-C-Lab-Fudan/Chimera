#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <assert.h>

#define MPEG2_BASE_ADDR 0x100000000
#define MPEG2_RESET_AND_SEQUENCE_CONTROL 0x000000000
#define MPEG2_VIDEO_FRAME_SIZE 0x000000008
#define MPEG2_OUT_BUF_CONTROL 0x000000010
#define MPEG2_BASE_IN_PIXELS 0x010000000
#define MPEG2_BASE_OUT_BUF 0x010000000

#define IN_BUF_SIZE 0x100000
#define OUT_BUF_SIZE 0x100000

uint8_t* registerPtr;

void writeReg(uint64_t offset_addr, uint64_t value)
{
    std::memcpy((registerPtr + offset_addr), &value, sizeof(uint64_t));
}

void mpeg2encoder_reset()
{
    writeReg(MPEG2_RESET_AND_SEQUENCE_CONTROL, 0x0);
    writeReg(MPEG2_RESET_AND_SEQUENCE_CONTROL, 0x1);
}

void mpeg2encoder_set_sequence_stop()
{
    writeReg(MPEG2_RESET_AND_SEQUENCE_CONTROL, 0x3);
}

void mpeg2encoder_finish_stop()
{
    writeReg(MPEG2_RESET_AND_SEQUENCE_CONTROL, 0x4);
}

void mpeg2encoder_set_video_frame_size(uint64_t value)
{
    writeReg(MPEG2_VIDEO_FRAME_SIZE, value);
}

void mpeg2encoder_put_pixels(void* buffer, uint64_t size)
{
    std::memcpy((registerPtr + MPEG2_BASE_IN_PIXELS), buffer, size);
}

void mpeg2encoder_get_outbuf(uint8_t* buffer, int* overflow, int* size)
{
    uint64_t local_ptr = 0;
    *overflow          = 0;
    while (true) {
        uint64_t status;
        std::memcpy(&status, registerPtr + MPEG2_OUT_BUF_CONTROL, sizeof(uint64_t));
        std::cout << "status:" << std::hex << status << std::dec << std::endl;
        if (status & 0x1) // valid flag
        {
            uint32_t size = static_cast<uint32_t>(status >> 32);
            std::cout << "ready data size:" << size << std::endl;
            std::memcpy(buffer + local_ptr, registerPtr + MPEG2_BASE_OUT_BUF, size);
            local_ptr += size;

            if (status & 0x2) {
                *overflow = 1;
                std::cout << "overflow flag valid" << std::endl;
                break;
            } else if (status & 0x4) {
                // finish flag
                break;
            } else if (size == 0) {
                break;
            }
        }
    }

    *size = local_ptr;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <string_argument>" << std::endl;
        return 1;
    }

    static uint8_t in_buffer[IN_BUF_SIZE];
    static uint8_t out_buffer[OUT_BUF_SIZE];

    size_t size   = 4096;
    void*  addr   = reinterpret_cast<void*>(0x100000000);
    int    prot   = PROT_READ | PROT_WRITE;
    int    flags  = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
    void*  result = mmap(addr, size, prot, flags, -1, 0);
    if (result == MAP_FAILED) {
        perror("Error mapping memory");
        return 1;
    }

    registerPtr = static_cast<uint8_t*>(result);

    std::cout << "ready to reset encoder" << std::endl;
    mpeg2encoder_reset();
    std::cout << "reset finished." << std::endl;

    // mpeg2encoder_set_video_frame_size
    std::cout << "ready to set video frame size" << std::endl;
    uint64_t reg_value;
    reg_value = 18; // xsize
    reg_value <<= 32;
    reg_value |= 13; // ysize
    mpeg2encoder_set_video_frame_size(reg_value);
    std::cout << "set video frame size finished." << std::endl;

    std::cout << "ready to read file" << std::endl;
    // read file
    // const char* in_file_name = "/home/gem5fpga/Desktop/workspace/gem5fpga/chimera/tests/test-progs/phy/288x208.raw";
    const char* in_file_name = argv[1];
    FILE*       in_fp        = fopen(in_file_name, "rb");
    if (in_fp == NULL) {
        printf("*** ERROR: failed to open file %s for read\n", in_file_name);
        exit(-1);
    }

    // const char* out_file_name = "/home/gem5fpga/Desktop/workspace/gem5fpga/chimera/tests/test-progs/phy/288x208.m2v";
    const char* out_file_name = argv[2];
    FILE*       out_fp        = fopen(out_file_name, "wb");
    if (out_fp == NULL) {
        printf("*** ERROR: failed to open file %s for read\n", out_file_name);
        exit(-1);
    }

    int acc_read_file_size  = 0;
    int acc_write_file_size = 0;

    std::cout << "ready to fill pixels......" << std::endl;
    do {
        int in_len, out_len, out_overflow;

        in_len = fread((void*)in_buffer, 1, IN_BUF_SIZE, in_fp);
        std::cout << std::hex << "read file length: " << in_len << ", acc length: " << acc_read_file_size << std::dec << std::endl;
        acc_read_file_size += in_len;

        mpeg2encoder_put_pixels((void*)in_buffer, in_len);

        if (feof(in_fp) || acc_read_file_size >= 0x600000) { mpeg2encoder_set_sequence_stop(); }

        mpeg2encoder_get_outbuf(out_buffer, &out_overflow, &out_len);

        std::cout << "overflow var value: " << out_overflow << std::endl;

        if (out_overflow) { printf("*** WARNING: device's out buffer overflow, out data stream is partly corrupted\n"); }

        assert(fwrite((void*)out_buffer, 1, out_len, out_fp) == out_len);
        std::cout << "write out file size: " << std::hex << out_len << std::dec << std::endl;

        acc_write_file_size += out_len;
    } while (!(feof(in_fp) || acc_read_file_size >= 0x600000));

    std::cout << "total output file size: " << std::hex << acc_write_file_size << std::dec << std::endl;

    if (munmap(result, size) == -1) { perror("Error unmapping memory"); }

    mpeg2encoder_finish_stop();

    return 0;
}