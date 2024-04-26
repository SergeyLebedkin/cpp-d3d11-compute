#include <cassert>
#include <iostream>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>

void CompileComputeShader(std::string_view source, ID3DBlob** shader) {
    // compile shader
    ID3DBlob* error{};
    D3DCompile(source.data(), source.length(), NULL, NULL, NULL, "main", "cs_5_0", 0, 0, shader, &error);
    // check compilation error
    if (error) {
        std::cout << (const char*)(error->GetBufferPointer()) << std::endl;
        error->Release();
    }
    // return compiled shader blob
    assert(shader);
}

void Texture2DReadData(ID3D11DeviceContext* context, ID3D11Texture2D* texSrc, void* buff) {
    assert(context);
    assert(texSrc);
    // get context device
    ID3D11Device* device{};
    context->GetDevice(&device);
    assert(device);
    // get texture description
    D3D11_TEXTURE2D_DESC texDesc{};
    texSrc->GetDesc(&texDesc);
    // create staging texture
    ID3D11Texture2D* texStaging{};
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.BindFlags = 0;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    device->CreateTexture2D(&texDesc, NULL, &texStaging);
    assert(texStaging);
    // copy texture data to staged texture and read data
    context->CopyResource(texStaging, texSrc);
    D3D11_MAPPED_SUBRESOURCE mappedSubres{};
    context->Map(texStaging, 0, D3D11_MAP_READ, 0, &mappedSubres);
    std::memcpy(buff, mappedSubres.pData, mappedSubres.DepthPitch);
    context->Unmap(texStaging, 0);
    // release handles
    texStaging->Release();
}

int main(int argc, char** argv) {
    // create device and context
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, NULL, &context);
    assert(device);
    assert(context);

    // create shader blob
    ID3DBlob* computeShaderBlob{};
    CompileComputeShader("RWTexture2D<float4> tex: register(u0); [numthreads(8, 8, 1)] void main(uint2 id: SV_DispatchThreadID) { tex[id] = float4(1.0, 0.5, 0.2, 0.0); }", &computeShaderBlob);
    assert(computeShaderBlob);
    // create compute shader
    ID3D11ComputeShader* computeShader{};
    device->CreateComputeShader(computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize(), NULL, &computeShader);
    assert(computeShader);

    // create texture
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 512;
    texDesc.Height = 512;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    ID3D11Texture2D* tex{};
    device->CreateTexture2D(&texDesc, NULL, &tex);
    assert(tex);
    // create texture unordered access view
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    ID3D11UnorderedAccessView* uavTexView{};
    device->CreateUnorderedAccessView(tex, &uavDesc, &uavTexView);
    assert(uavTexView);

    // execute compute shader
    context->CSSetShader(computeShader, NULL, 0);
    context->CSSetUnorderedAccessViews(0, 1, &uavTexView, NULL);
    context->Dispatch(texDesc.Width / 8, texDesc.Height / 8, 1);

    // read texture data
    UINT8* texData = new UINT8[512 * 512 * 4]{};
    Texture2DReadData(context, tex, texData);
    delete[] texData;

    // release texture
    uavTexView->Release();
    tex->Release();

    // release compute shader and blob
    computeShader->Release();
    computeShaderBlob->Release();

    // release device and context
    context->Release();
    device->Release();

    return 0;
}
