
add_executable (ImageProcessor 
"base64.h"
"main.cpp"
)


target_link_libraries (ImageProcessor
Boost::system
Boost::thread
Boost::json
${OpenCV_LIBS}
ws2_32
)


