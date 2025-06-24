add_executable (Server 
"main.cpp"
"base64.h"
)

target_link_libraries (Server
Boost::system
Boost::json
${OpenCV_LIBS}
)

if (WIN32)
    target_link_libraries (Server 
        ws2_32
        mswsock
    )
endif()