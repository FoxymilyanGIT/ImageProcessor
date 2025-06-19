add_executable (Server 
"main.cpp"
"base64.h"
)

target_link_libraries (Server
Boost::system
Boost::thread
Boost::json
${OpenCV_LIBS}
ws2_32
)


