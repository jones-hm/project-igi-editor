# IGI Editor

A 3D level editor for the IGI game project, now with FBX support and embedded textures.

## Overview

IGI Editor has been migrated from OBJ format to FBX format using the Assimp library. This change provides better model support, embedded textures, and a unified asset folder structure.

## New Folder Structure

The project now uses a unified `models/` directory instead of separate folders:

```
QEditor/
├── models/              # All FBX models with embedded textures
│   ├── level1/         # Level-specific models
│   ├── level2/         # Level-specific models
│   └── ...             # Other level directories
├── terrains/           # Terrain data (unchanged)
└── ...                 # Other QEditor files
```

### Previous Structure (Deprecated)
```
QEditor/
├── objects/            # OBJ models (deprecated)
├── buildings/          # Building OBJ models (deprecated)
├── textures/           # Separate texture files (deprecated)
│   ├── level1/         # Level-specific textures
│   ├── level2/         # Level-specific textures
│   └── ...             # Other level directories
└── ...                 # Other QEditor files
```

## Migration Benefits

1. **Unified Asset Management**: All models are now in a single `models/` directory
2. **Embedded Textures**: Textures are embedded directly in FBX files, eliminating external texture dependencies
3. **Better Format Support**: FBX provides better compatibility with modern 3D tools
4. **Simplified File Management**: No need to manage separate texture folders and naming conventions

## Technical Changes

### Model Loading
- **Previous**: Used TinyOBJ loader with separate texture file resolution
- **Current**: Uses Assimp library with embedded texture support

### File Search Priority
The system now searches for FBX files in this order:
1. `models/levelX/modelname.fbx` (level-specific)
2. `models/modelname.fbx` (general)

### Texture Handling
- Textures are now embedded directly in FBX files
- No external texture path resolution is needed
- Supports various texture formats (PNG, JPG, TGA, etc.)

## Conversion Tools

### Blender Batch Conversion Script
A Python script is provided to convert existing OBJ models to FBX format:

```bash
# Convert single directory
python tools/convert_to_fbx.py --input-dir /path/to/objects --output-dir /path/to/models --texture-dir /path/to/textures

# Batch convert entire QEditor structure
python tools/convert_to_fbx.py --batch --base-dir /path/to/QEditor
```

**Requirements:**
- Blender 3.0+
- Python 3.6+

### Script Features
- Converts OBJ files to FBX with embedded textures
- Automatically finds corresponding texture files
- Handles multiple texture formats
- Preserves UV coordinates and materials
- Batch processing support

## Building the Project

### Dependencies
- CMake 3.16+
- OpenGL 4.1+
- Assimp library (included via CMake)
- Modern C++ compiler

### Build Steps
```bash
mkdir build
cd build
cmake ..
make  # or Visual Studio on Windows
```

## Usage

1. **Convert Existing Assets**: Use the Blender script to convert OBJ models to FBX
2. **Update Model Paths**: Ensure all model references point to the new `models/` directory
3. **Run the Editor**: The editor will automatically load FBX models with embedded textures

## File Format Support

### Supported Input Formats
- FBX (.fbx) - Primary format
- OBJ (.obj) - Legacy support (deprecated)

### Supported Texture Formats
- PNG (.png)
- JPEG (.jpg, .jpeg)
- TGA (.tga)
- BMP (.bmp)
- TIFF (.tiff)

## Configuration

The editor uses the following configuration files:
- `IGIModelsLevel.json` - Model metadata and level assignments
- Standard IGI configuration files

## Troubleshooting

### Common Issues

1. **Models Not Loading**
   - Check that FBX files are in the correct `models/` directory
   - Ensure file names match the expected model IDs
   - Verify that textures are properly embedded in FBX files

2. **Missing Textures**
   - Re-convert the FBX file ensuring textures are embedded
   - Check that original texture files exist during conversion
   - Verify texture file names match the model names

3. **Conversion Errors**
   - Ensure Blender is properly installed
   - Check that input OBJ files are valid
   - Verify texture files exist and are readable

### Debug Information

The editor logs detailed information about model loading:
- Check the console output for loading status
- Look for texture embedding warnings
- Verify file paths in the logs

## Development

### Adding New Models
1. Create or convert your model to FBX format
2. Ensure textures are embedded in the FBX file
3. Place the FBX file in the appropriate `models/levelX/` directory
4. Update model metadata in `IGIModelsLevel.json` if needed

### Modifying the Model Loader
The model loading logic is implemented in:
- `source/model_loader.h/.cpp` - Core FBX loading with Assimp
- `source/renderer/renderer_objects.cpp` - Model caching and rendering

## License

This project is licensed under the same terms as the original IGI Editor.

## Contributing

When contributing to this project:
1. Ensure all new models use FBX format with embedded textures
2. Update documentation for any folder structure changes
3. Test with the provided conversion tools
4. Follow the existing code style and patterns