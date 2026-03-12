#include "Sprite.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <filesystem>

// Simple helper function to load an image into a OpenGL texture with common settings
bool Sprite::LoadTextureFromFile(const char* filename)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    std::filesystem::path resourcePath = std::filesystem::path("resources") / filename;
    std::string newFilename = resourcePath.string();
    unsigned char* image_data = stbi_load(newFilename.c_str(), &image_width, &image_height, NULL, 4);
    if (image_data == NULL) {
        _size = ImVec2(0, 0);
        std::cout << "Failed to load texture: " << newFilename << std::endl;
        return false;
    }
    _texture = _loadTextureFromMemory(image_data, image_width, image_height);
    stbi_image_free(image_data);
    if (_texture == 0) {
        _size = ImVec2(0, 0);
        return false;
    }
    _size = ImVec2((float)image_width, (float)image_height);
    return true;
}

void Sprite::setHighlighted(bool highlighted)
{
	if (highlighted != _highlighted) {
		_highlighted = highlighted;
	}
}

bool Sprite::highlighted()
{
	return _highlighted;
}

#ifdef __APPLE__
#include "../imgui/imgui_impl_opengl3_loader.h"

ImTextureID Sprite::_loadTextureFromMemory(const unsigned char *image_data, int image_width, int image_height)
{
    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    return static_cast<ImTextureID>(image_texture);
}

#else

// DirectX
#include <stdio.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

ImTextureID Sprite::_loadTextureFromMemory(const unsigned char *image_data, int image_width, int image_height)
{
    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D *pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    // You need to have a valid ID3D11Device* available as g_pd3dDevice
    extern ID3D11Device* g_pd3dDevice; // Add this line if g_pd3dDevice is defined elsewhere

    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
    if (FAILED(hr) || !pTexture) {
        return 0;
    }

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    hr = g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &shaderResourceView);
    pTexture->Release();

    if (FAILED(hr) || !shaderResourceView) {

        return 0;
    }
    return reinterpret_cast<ImTextureID>(shaderResourceView);
}
#endif

