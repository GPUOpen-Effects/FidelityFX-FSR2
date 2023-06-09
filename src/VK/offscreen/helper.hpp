#pragma once
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <FindFiles.h>
#include <iostream>

struct config
{
    std::vector<std::string> file_list;
    std::string input_path = "";
    std::string motion_depth_path = "";
    std::string out_path = "";
    float scale_motion_x = 1.0f;
    float scale_motion_y = 1.0f;
    float scale_depth = 1.0f;
    double deltaTime = 33.3f;
    int outWidth = 1;
    int outHeight = 1;
    bool reset = false;
    bool enableJitter = false;
};

bool get_files(std::vector<std::string>& fileNames, std::string& input_path)
{
    FindFiles finder;
    fileNames = finder.findFiles(input_path.c_str());
    if (fileNames.empty()) return false;
    std::sort(fileNames.begin(), fileNames.end());
    return true;
}

std::vector<float> read_image_exr(const std::string& file_path, int& w, int& h,int channel) 
{
    float* data = nullptr;
    const char* err = nullptr;
    int ret = LoadEXR(&data, &w, &h, file_path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            std::cout << "EXR reading error: " << err << "\n";
            FreeEXRErrorMessage(err);
        }
    }
    std::vector<float> data_rgba{ data ,data+(w*h)*channel}; // auto free
    return data_rgba;
}

void write_exr(const std::string& _file_path, std::vector<float> &data, int w, int h, int _channels=4,
    bool use_half = false) 
{
    if (data.empty())
        return;
    std::string file_path = _file_path;

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = _channels;

    int pixel_n = w * h;
    std::vector<float> images[4];
    images[0].resize(pixel_n);
    images[1].resize(pixel_n);
    images[2].resize(pixel_n);
    images[3].resize(pixel_n);

    for (int i = 0; i < pixel_n; i++) {
        images[0][i] = data[4 * i + 0];
        images[1][i] = data[4 * i + 1];
        images[2][i] = data[4 * i + 2];
        images[3][i] = data[4 * i + 3];
    }

    float* image_ptr[4];
    image_ptr[0] = images[3].data(); // A               
    image_ptr[1] = images[2].data(); // B
    image_ptr[2] = images[1].data(); // G
    image_ptr[3] = images[0].data(); // R


    image.images = (uint8_t**)image_ptr;
    image.width = w;
    image.height = h;

    header.num_channels = 4;
    auto channels = std::make_unique<EXRChannelInfo[]>(header.num_channels);
    header.channels = channels.get();
    // Must be (A)BGR order, since most of EXR viewers expect this channel order
    strncpy_s(header.channels[0].name, "A", 255);
    header.channels[0].name[strlen("A")] = '\0';
    strncpy_s(header.channels[1].name, "B", 255);
    header.channels[1].name[strlen("B")] = '\0';
    strncpy_s(header.channels[2].name, "G", 255);
    header.channels[2].name[strlen("G")] = '\0';
    strncpy_s(header.channels[3].name, "R", 255);
    header.channels[3].name[strlen("R")] = '\0';

    auto pixel_types = std::make_unique<int[]>(header.num_channels);
    header.pixel_types = pixel_types.get();
    auto requested_pixel_types = std::make_unique<int[]>(header.num_channels);
    header.requested_pixel_types = requested_pixel_types.get();
    for (int i = 0; i < header.num_channels; i++) {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] =
            use_half ? // pixel type of output image to be stored in EXR file
            TINYEXR_PIXELTYPE_HALF :
            TINYEXR_PIXELTYPE_FLOAT;
    }

    header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;

    const char* err = nullptr;
    int ret = SaveEXRImageToFile(&image, &header, file_path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            std::cout << "EXR writing error: " << err << std::endl;
            FreeEXRErrorMessage(err);
        }
        return;
    };
}