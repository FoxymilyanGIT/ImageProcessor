# ImageProcessor

ImageProcessor is an HTTP server and client application written in C++. The server receives JSON requests containing a base64-encoded .jpeg image and a text string, then overlays the text onto the image using OpenCV. The processed image is returned in base64 format within a JSON response. The project also includes a client utility for user-friendly interaction with the server, handling image encoding/decoding and file operations.

## Features

- **Server:**
  - Accepts HTTP requests with JSON payloads
  - Decodes images from base64
  - Overlays custom text onto the image using OpenCV
  - Sends back the processed image as base64 in JSON

- **Client:**
  - Requests user input for a text string, image path, and save path
  - Reads image files and encodes them to base64
  - Sends JSON requests to the server with the text and encoded image
  - Receives the processed image in base64 and saves it to the specified path
  - Converts images between base64 and .jpeg

- Built with C++, [Boost](https://www.boost.org) and [OpenCV](https://opencv.org/)

## Technology Stack

- C++
- Boost (HTTP server, JSONs, thread_pool and ASIO)
- OpenCV (image encoding and manipulation)

## Build Instructions

This project is built using CMake and [vcpkg](https://vcpkg.io/) for dependency management.

### Prerequisites

- C++ compiler (C++17 or newer recommended)
- [CMake](https://cmake.org/) (version 3.16+)
- [vcpkg](https://github.com/microsoft/vcpkg)

### Steps

1. **Install vcpkg dependencies:**
   ```sh
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat 
   vcpkg install boost opencv
   ```

2. **Clone this repository and create build directory:**
   ```sh
   git clone <repo-url>
   cd ImageProcessor
   mkdir build && cd build
   ```

3. **Configure with CMake:**
   ```sh
   cmake -DCMAKE_TOOLCHAIN_FILE=</path/to/vcpkg/scripts/buildsystems/vcpkg.cmake> ..
   ```

4. **Build the project:**
   ```sh
   cmake --build .
   ```

## Usage

### Server

1. Start the server:
   
Navigate to the build/Server and run Server.exe

3. The server listens for POST requests with the following JSON structure:
   ```json
   {
     "image": "<base64-encoded-image>",
     "text": "Your custom text"
   }
   ```

4. The server responds with:
   ```json
   {
     "result": "<base64-encoded-processed-image or message>"
   }
   ```

### Client

1. Run the client:

Navigate to the build/Client and run Client.exe

2. The client will prompt you for:
   - The text you want to add to the image
   - The path to the input image file
   - The path where the processed image should be saved

3. The client will:
   - Read and encode the image file as base64
   - Send a JSON request to the server
   - Receive the processed image in base64
   - Decode and save the image to the specified path

#### Example with curl (for manual testing)

```sh
curl -X POST http://localhost:8000/process \
     -H "Content-Type: application/json" \
     -d '{"image":"<base64>","text":"Sample Text"}'
```
