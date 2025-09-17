# Ship of Harkinian - Web Build

This document describes how to build and deploy Ship of Harkinian for web browsers using Emscripten.

## Prerequisites

1. **Emscripten SDK**: Install the Emscripten SDK (emsdk)
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **Build Tools**:
   - CMake (3.26+)
   - Ninja build system
   - Python 3
   - Git

3. **System Dependencies** (Linux/macOS):
   ```bash
   # Ubuntu/Debian
   sudo apt-get install cmake ninja-build python3 libpng-dev

   # macOS (via Homebrew)
   brew install cmake ninja python3
   ```

## Building

### Quick Build

Use the provided build script:
```bash
./build-emscripten.sh
```

### Manual Build

1. **Clone and setup the repository**:
   ```bash
   git clone https://github.com/HarbourMasters/Shipwright.git
   cd Shipwright
   git submodule update --init
   ```

2. **Configure with Emscripten**:
   ```bash
   mkdir build-emscripten
   cd build-emscripten

   emcmake cmake .. \
     -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DBUILD_CROWD_CONTROL=OFF \
     -DBUILD_REMOTE_CONTROL=OFF
   ```

3. **Build the project**:
   ```bash
   # Build custom assets
   cmake --build . --target GenerateSohOtr

   # Build the main executable
   cmake --build . --config Release
   ```

4. **Files generated**:
   - `soh.html` - Main HTML file
   - `soh.js` - JavaScript loader
   - `soh.wasm` - WebAssembly binary
   - `soh.data` - Preloaded data file

## Deployment

### Local Testing

1. **Start a local web server**:
   ```bash
   cd web-dist
   python3 -m http.server 8000
   ```

2. **Open in browser**:
   Navigate to `http://localhost:8000`

### Production Deployment

#### GitHub Pages

The GitHub Actions workflow automatically deploys to GitHub Pages when pushing to the `main` branch.

#### Manual Deployment

Upload these files to your web server:
- `index.html`
- `soh.js`
- `soh.wasm`
- `soh.data`
- `soh.o2r` (if available)
- `oot.o2r` (user must provide or generate)

### Web Server Configuration

#### CORS Headers
Ensure your web server sends proper CORS headers:
```
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Opener-Policy: same-origin
```

#### MIME Types
Configure correct MIME types:
```
.wasm  → application/wasm
.o2r   → application/octet-stream
```

#### Example nginx configuration:
```nginx
location / {
    add_header Cross-Origin-Embedder-Policy "require-corp";
    add_header Cross-Origin-Opener-Policy "same-origin";

    location ~ \.wasm$ {
        add_header Content-Type "application/wasm";
    }
}
```

## Features

### Supported
- OpenGL ES 3.0 rendering via WebGL 2.0
- SDL2 input handling (keyboard/mouse)
- Persistent save data via IndexedDB
- Full screen support
- Gamepad support (via Gamepad API)

### Limitations
- No network features (crowd control, remote control)
- Audio may have latency depending on browser
- Performance varies by browser and hardware
- File size is larger due to WASM overhead

## Browser Requirements

### Minimum Requirements
- WebGL 2.0 support
- WebAssembly support
- IndexedDB support
- Modern JavaScript (ES6+)

### Recommended Browsers
- Chrome/Chromium 90+
- Firefox 89+
- Safari 15+ (macOS/iOS)
- Edge 90+

## Asset Loading

The web build can load assets in several ways:

1. **Pre-packaged**: Assets included in the build via `--preload-file`
2. **Runtime Download**: Assets fetched from server on startup
3. **User Upload**: ROM file uploaded via file input

### ROM File Handling

Users can provide ROM files through the web interface:
1. Click "Load ROM File" button
2. Select a compatible ROM (.z64, .n64, .v64)
3. The game will extract assets and generate OTR files

## Performance Optimization

### Build Optimizations
- Use `-O3` optimization level for release builds
- Enable LTO (Link Time Optimization) if supported
- Strip debug symbols for smaller file size

### Runtime Optimizations
- Adjust memory settings in CMake:
  ```cmake
  -s INITIAL_MEMORY=256MB  # Lower initial memory
  -s MAXIMUM_MEMORY=2GB    # Adjust based on needs
  ```

### CDN Deployment
- Serve static files from CDN
- Enable gzip/brotli compression
- Set appropriate cache headers

## Troubleshooting

### Common Issues

1. **"Out of memory" errors**:
   - Increase `INITIAL_MEMORY` in build settings
   - Check browser memory limits

2. **"WebGL context lost"**:
   - Reduce graphics quality settings
   - Check GPU driver updates

3. **Input not working**:
   - Click on canvas to focus
   - Check browser permissions

4. **Save data not persisting**:
   - Check IndexedDB quota
   - Clear browser storage and retry

### Debug Build

For debugging, build with:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

This enables:
- Assertion checks
- Debug symbols
- Console logging

## CI/CD Integration

The repository includes a GitHub Actions workflow (`emscripten-build.yml`) that:
1. Builds the web version on every push
2. Uploads artifacts for testing
3. Optionally deploys to GitHub Pages

To enable GitHub Pages deployment:
1. Go to Settings → Pages
2. Set source to "GitHub Actions"
3. Push to `main` branch

## Contributing

When contributing web-specific features:
1. Test in multiple browsers
2. Check mobile device compatibility
3. Minimize additional JavaScript dependencies
4. Follow Emscripten best practices
5. Update this documentation as needed

## License

Ship of Harkinian is licensed under the MIT License. See the main LICENSE file for details.