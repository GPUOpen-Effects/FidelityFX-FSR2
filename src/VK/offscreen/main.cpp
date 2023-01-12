#include "Vulkan_FSR2.hpp"

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: FSR2 OffScreen [OPTIONS]\n\n";
        std::cout << "OPTIONS:\n";
        std::cout << "  --color_path <input_path> path of input color images\n";
        std::cout << "  --mv_d_path <motion_depth_path> path of motion vector and depth of input images\n";
        std::cout << "  --out_path <out_path>   the output path of the result images\n";
        std::cout << "  --s_mv_x <number>  the scale of x of motion vector\n";
        std::cout << "  --s_mv_y <number>  the scale of y of motion vector\n";
        std::cout << "  --s_depth <number>  the scale of depth\n";
        std::cout << "  --fps <number>  Frames Per Second\n";
        std::cout << "  --w <number>  output Width\n";
        std::cout << "  --h <number>  output Height\n";
        std::cout << "  --r  reset for every frame\n";
        std::cout << "  --j  enable Jitter\n";

        return 0;
    }
    std::vector<std::string> file_list;
    std::string input_path = "";
    std::string motion_depth_path = "";
    std::string out_path = "";
    float scale_motion_x = -0.5f;//default value cause I am working with ue4.
    float scale_motion_y = 0.5f;
    float scale_depth = 1000.0f;
    double deltaTime = 33.3f;//33.3f for 30fps, if running at 60fps, the value passed should be around 16.6f.
    float outWidth = 3840;
    float outHeight = 2160;
    bool reset = false;
    bool enableJitter = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--color_path") == 0 && i + 1 < argc) 
        {
            input_path = argv[i + 1];
        }
        else if (strcmp(argv[i], "--mv_d_path") == 0 && i + 1 < argc) 
        {
            motion_depth_path = argv[i + 1];
        }
        else if (strcmp(argv[i], "--out_path") == 0 && i + 1 < argc) 
        {
            out_path = argv[i + 1];
        }
        else if (strcmp(argv[i], "--s_mv_x") == 0 && i + 1 < argc)
        {
            scale_motion_x = atof(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--s_mv_y") == 0 && i + 1 < argc) 
        {
            scale_motion_y = atof(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--s_depth") == 0 && i + 1 < argc)
        {
            scale_depth = atof(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc)
        {
            double fps = atof(argv[i + 1]);
            deltaTime = 1.0f / fps;
        }
        else if (strcmp(argv[i], "--w") == 0 && i + 1 < argc)
        {
            outWidth = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--h") == 0 && i + 1 < argc)
        {
            outHeight = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--r") == 0)
        {
            reset = true;
        }
        else if (strcmp(argv[i], "--j") == 0)
        {
            enableJitter = true;
        }
    }
    get_files(file_list, input_path);
    //copy
    config config_fsr;
    config_fsr.file_list = file_list;
    config_fsr.input_path = input_path;
    config_fsr.motion_depth_path = motion_depth_path;
    config_fsr.out_path = out_path;
    config_fsr.scale_motion_x = scale_motion_x;
    config_fsr.scale_motion_y = scale_motion_y;
    config_fsr.scale_depth = scale_depth;
    config_fsr.deltaTime = deltaTime;
    config_fsr.outWidth = outWidth;
    config_fsr.outHeight = outHeight;
    config_fsr.reset = reset;
    config_fsr.enableJitter = enableJitter;

    Vulkan_FSR2 FSR2;
    FSR2.run(config_fsr);
    

    return EXIT_SUCCESS;
}