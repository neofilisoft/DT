#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tinygltf/tiny_gltf.h>

namespace fs = std::filesystem;

struct AssetHeader {
    char magic[4]; // "DTAS"
    uint32_t version; // 1
    uint32_t type; // 1 = Texture, 2 = Mesh
};

struct TexturePayloadHeader {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
};

struct Vertex {
    float position[3];
    float normal[3];
    float texCoord[2];
};

struct MeshPayloadHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
};

bool CookTexture(const std::string& inputPath, const std::string& outputPath)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    // Load image as RGBA
    unsigned char* pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 4);
    if (!pixels)
    {
        std::cerr << "Error: failed to load image " << inputPath << " (" << stbi_failure_reason() << ")\n";
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        std::cerr << "Error: failed to open output file " << outputPath << "\n";
        stbi_image_free(pixels);
        return false;
    }

    // Write header
    AssetHeader header;
    header.magic[0] = 'D';
    header.magic[1] = 'T';
    header.magic[2] = 'A';
    header.magic[3] = 'S';
    header.version = 1;
    header.type = 1; // Texture
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write payload header
    TexturePayloadHeader texHeader;
    texHeader.width = static_cast<uint32_t>(width);
    texHeader.height = static_cast<uint32_t>(height);
    texHeader.channels = 4; // We forced 4 channels (RGBA)
    out.write(reinterpret_cast<const char*>(&texHeader), sizeof(texHeader));

    // Write raw pixels
    out.write(reinterpret_cast<const char*>(pixels), width * height * 4);

    stbi_image_free(pixels);
    std::cout << "Successfully cooked texture: " << inputPath << " -> " << outputPath << " (" << width << "x" << height << ")\n";
    return true;
}

bool CookMesh(const std::string& inputPath, const std::string& outputPath)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    if (fs::path(inputPath).extension() == ".glb")
    {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, inputPath);
    }
    else
    {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, inputPath);
    }

    if (!warn.empty())
    {
        std::cout << "Warning: " << warn << "\n";
    }

    if (!ret)
    {
        std::cerr << "Error: failed to parse glTF: " << err << "\n";
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Parse the first mesh in the model
    if (model.meshes.empty())
    {
        std::cerr << "Error: glTF file contains no meshes.\n";
        return false;
    }

    const auto& mesh = model.meshes[0];
    for (const auto& primitive : mesh.primitives)
    {
        // Extract indices
        const auto& indexAccessor = model.accessors[primitive.indices];
        const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const auto& indexBuffer = model.buffers[indexBufferView.buffer];
        
        size_t indexCount = indexAccessor.count;
        const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

        size_t startIndex = vertices.size();

        // Access positions
        int posAccessorIdx = primitive.attributes.at("POSITION");
        const auto& posAccessor = model.accessors[posAccessorIdx];
        const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
        const auto& posBuffer = model.buffers[posBufferView.buffer];
        const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

        // Access normals
        const float* normalData = nullptr;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
        {
            int normAccessorIdx = primitive.attributes.at("NORMAL");
            const auto& normAccessor = model.accessors[normAccessorIdx];
            const auto& normBufferView = model.bufferViews[normAccessor.bufferView];
            const auto& normBuffer = model.buffers[normBufferView.buffer];
            normalData = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
        }

        // Access uvs
        const float* uvData = nullptr;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
        {
            int uvAccessorIdx = primitive.attributes.at("TEXCOORD_0");
            const auto& uvAccessor = model.accessors[uvAccessorIdx];
            const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
            const auto& uvBuffer = model.buffers[uvBufferView.buffer];
            uvData = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
        }

        for (size_t i = 0; i < posAccessor.count; ++i)
        {
            Vertex vertex{};
            vertex.position[0] = posData[i * 3 + 0];
            vertex.position[1] = posData[i * 3 + 1];
            vertex.position[2] = posData[i * 3 + 2];

            if (normalData)
            {
                vertex.normal[0] = normalData[i * 3 + 0];
                vertex.normal[1] = normalData[i * 3 + 1];
                vertex.normal[2] = normalData[i * 3 + 2];
            }

            if (uvData)
            {
                vertex.texCoord[0] = uvData[i * 2 + 0];
                vertex.texCoord[1] = uvData[i * 2 + 1];
            }

            vertices.push_back(vertex);
        }

        // Process indices
        if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
        {
            const uint16_t* idxs = reinterpret_cast<const uint16_t*>(indexData);
            for (size_t i = 0; i < indexCount; ++i)
            {
                indices.push_back(static_cast<uint32_t>(idxs[i] + startIndex));
            }
        }
        else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
        {
            const uint32_t* idxs = reinterpret_cast<const uint32_t*>(indexData);
            for (size_t i = 0; i < indexCount; ++i)
            {
                indices.push_back(idxs[i] + static_cast<uint32_t>(startIndex));
            }
        }
        else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE)
        {
            const uint8_t* idxs = reinterpret_cast<const uint8_t*>(indexData);
            for (size_t i = 0; i < indexCount; ++i)
            {
                indices.push_back(static_cast<uint32_t>(idxs[i] + startIndex));
            }
        }
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        std::cerr << "Error: failed to open output file " << outputPath << "\n";
        return false;
    }

    // Write header
    AssetHeader header;
    header.magic[0] = 'D';
    header.magic[1] = 'T';
    header.magic[2] = 'A';
    header.magic[3] = 'S';
    header.version = 1;
    header.type = 2; // Mesh
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write payload header
    MeshPayloadHeader meshHeader;
    meshHeader.vertexCount = static_cast<uint32_t>(vertices.size());
    meshHeader.indexCount = static_cast<uint32_t>(indices.size());
    out.write(reinterpret_cast<const char*>(&meshHeader), sizeof(meshHeader));

    // Write vertex array
    out.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));

    // Write index array
    out.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));

    std::cout << "Successfully cooked mesh: " << inputPath << " -> " << outputPath << " (" << vertices.size() << " vertices, " << indices.size() << " indices)\n";
    return true;
}

struct ShaderPayloadHeader {
    uint32_t byteSize;
    uint32_t stage; // 0 = Vertex, 1 = Fragment, 2 = Compute
};

bool CookShader(const std::string& inputPath, const std::string& outputPath)
{
    // Determine shader stage from extension
    fs::path inPath(inputPath);
    std::string ext = inPath.extension().string();
    uint32_t stage = 0; // Default vertex
    if (ext == ".vert") stage = 0;
    else if (ext == ".frag") stage = 1;
    else if (ext == ".comp") stage = 2;
    else
    {
        std::cerr << "Error: Unknown shader extension '" << ext << "'. Expected .vert, .frag, or .comp\n";
        return false;
    }

    // Call glslc
    std::string tempSpv = outputPath + ".spv";
    std::string cmd = "glslc \"" + inputPath + "\" -o \"" + tempSpv + "\"";
    int res = std::system(cmd.c_str());
    if (res != 0)
    {
        std::cerr << "Error: glslc compilation failed for " << inputPath << "\n";
        return false;
    }

    // Read the compiled SPIR-V
    std::ifstream spvFile(tempSpv, std::ios::binary | std::ios::ate);
    if (!spvFile)
    {
        std::cerr << "Error: Failed to read temporary SPIR-V file " << tempSpv << "\n";
        return false;
    }

    std::streamsize size = spvFile.tellg();
    spvFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!spvFile.read(buffer.data(), size))
    {
        std::cerr << "Error: Failed to read SPIR-V data\n";
        return false;
    }
    spvFile.close();
    fs::remove(tempSpv);

    // Write .asset file
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        std::cerr << "Error: failed to open output file " << outputPath << "\n";
        return false;
    }

    // Write header
    AssetHeader header;
    header.magic[0] = 'D';
    header.magic[1] = 'T';
    header.magic[2] = 'A';
    header.magic[3] = 'S';
    header.version = 1;
    header.type = 3; // Shader
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write payload header
    ShaderPayloadHeader shaderHeader;
    shaderHeader.byteSize = static_cast<uint32_t>(size);
    shaderHeader.stage = stage;
    out.write(reinterpret_cast<const char*>(&shaderHeader), sizeof(shaderHeader));

    // Write raw SPIR-V bytes
    out.write(buffer.data(), buffer.size());

    std::cout << "Successfully cooked shader: " << inputPath << " -> " << outputPath << " (" << buffer.size() << " bytes)\n";
    return true;
}

int main(int argc, char* argv[])
{
    std::string input;
    std::string output;
    std::string type;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc)
        {
            input = argv[++i];
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            output = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc)
        {
            type = argv[++i];
        }
    }

    if (input.empty() || output.empty() || type.empty())
    {
        std::cerr << "DTCooker - DTEngine Asset Cooker\n";
        std::cerr << "Usage: DTCooker -i <input_file> -o <output_file> -t <type: texture|mesh|shader>\n";
        return 1;
    }

    if (type == "texture")
    {
        return CookTexture(input, output) ? 0 : 1;
    }
    else if (type == "mesh")
    {
        return CookMesh(input, output) ? 0 : 1;
    }
    else if (type == "shader")
    {
        return CookShader(input, output) ? 0 : 1;
    }
    else
    {
        std::cerr << "Error: unknown type '" << type << "'\n";
        return 1;
    }
}
