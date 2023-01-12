#include "Vulkan_FSR2.hpp"

int main() {
 
    std::vector<std::string> file_list = {"1045.exr","1046.exr","1047.exr","1048.exr","1049.exr"};
    std::string input_path = "D:/stuff/ue_stuffs/showdown_out/test2/Input/";
    std::string motion_depth_path = "D:/stuff/ue_stuffs/showdown_out/test2/RawMotionDepth/";
    std::string out_path = "D:/workwork/sssr/FSR2output/";

    Vulkan_FSR2 FSR2;
    FSR2.run(file_list, input_path, motion_depth_path, out_path);
    

    return EXIT_SUCCESS;
}