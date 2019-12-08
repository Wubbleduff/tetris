

#include "windows_renderer.h"
#include "game_renderer.h"

#include "my_math.h" // v2

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

#include <vector>

#include <assert.h>








// Renderer target info
struct Window
{
  HWND handle;

  unsigned framebuffer_width;
  unsigned framebuffer_height;
  float aspect_ratio;
  bool vsync;
  bool fullscreen;

  float background_color[4];
};

// DirectX state
struct D3DResources
{
  unsigned video_card_memory_bytes;
  unsigned char video_card_description[128];
  IDXGISwapChain *swap_chain;
  ID3D11Device *device;
  ID3D11DeviceContext *device_context;
  ID3D11RenderTargetView *render_target_view;
  ID3D11Texture2D *depth_stencil_buffer;
  ID3D11DepthStencilState *depth_stencil_state;
  ID3D11DepthStencilState *no_depth_stencil_state;
  ID3D11DepthStencilView *depth_stencil_view;
  ID3D11RasterizerState *raster_state;
  ID3D11RasterizerState *wireframe_raster_state;

  ID3D11Texture2D *render_target_texture;
  ID3D11RenderTargetView *render_to_texture_target_view;
  ID3D11ShaderResourceView *render_target_shader_resource_view;
};

struct Mesh
{
  struct Vertex
  {
    v3 position;
    v2 uv;

    Vertex() : position(v3()), uv(v2()) {}
    Vertex(v3 a, v2 b) : position(a), uv(b) {}
  };

  std::vector<Vertex> vertices;
  //std::vector<v3> vertices;
  //std::vector<v3> normals;
  //std::vector<v3> materials;
  //std::vector<v2> uvs;
  std::vector<unsigned> indices;


  ID3D11Buffer *vertex_buffer;
  ID3D11Buffer *index_buffer;


  unsigned draw_mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


  void normalize();
  void compute_vertex_normals();
  void fill_buffers(ID3D11Device *device);
  void update_data(ID3D11DeviceContext *device_context, void *data, unsigned num_bytes);
  void clear_buffers();
};

// Shaders
struct Shader
{
  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11InputLayout *layout;

  ID3D11Buffer *global_buffer;
};

struct FirstShaderBuffer
{
  mat4 world_m_model;
  mat4 view_m_world;
  mat4 clip_m_view;

  v4 color;
};

// Textures
struct Texture
{
  ID3D11ShaderResourceView *resource;
  ID3D11SamplerState *sample_state;
};

struct CellData
{
  v2i position;
  Color color;
};

struct RendererData
{
  Window window;
  D3DResources resources;

  FILE *shader_errors_file = 0;
  Shader quad_shader;

  ID3D11Buffer *first_shader_buffer;

  Mesh quad_mesh;
  Texture quad_texture;

  std::vector<CellData> cells_to_render;
  std::vector<CellData> left_bar_cells_to_render;
  std::vector<CellData> right_bar_cells_to_render;



  float grid_pixels_height;
  float grid_pixels_width;
  float bar_width;
};

// TODO: This is global. Move it somewhere nice.
static RendererData *renderer_data;
static const float GRID_WIDTH = 10.0f;
static const float GRID_HEIGHT = 24.0f;






static void output_shader_errors(ID3D10Blob *error_message, const char *shader_file)
{
  char *compile_errors;
  unsigned buffer_size;

  // Get a pointer to the error message text buffer.
  compile_errors = (char *)(error_message->GetBufferPointer());

  // Get the length of the message.
  buffer_size = error_message->GetBufferSize();

  // Write out the error message.
  compile_errors[buffer_size - 1] = '\0';

  fprintf(renderer_data->shader_errors_file, compile_errors);

  // Stop program when there's an error
  assert(0);

  // Release the error message.
  error_message->Release();
  error_message = 0;



  // Pop a message up on the screen to notify the user to check the text file for compile errors.
  //MessageBox(hwnd, "Error compiling shader.  Check shader-error.txt for message.", shader_file, MB_OK);
}

static void create_shader(const char *vs_path, const char *vs_name, const char *ps_path, const char *ps_name, D3D11_INPUT_ELEMENT_DESC *common_input_vertex_layout, unsigned num_elements, Shader *output_shader, ID3D11Buffer *in_buffer)
{

  ID3D11Device *device = renderer_data->resources.device;

  ID3D10Blob *error_message = 0;
  ID3D10Blob *vertex_shader_buffer = 0;
  ID3D10Blob *pixel_shader_buffer = 0;

  // Compile the vertex shader code.
  HRESULT result = D3DX11CompileFromFile(vs_path, NULL, NULL, vs_name, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, NULL, 
    &vertex_shader_buffer, &error_message, NULL);
  if(FAILED(result))
  {
    if(error_message) output_shader_errors(error_message, vs_path);
    else assert(0); // Could not find shader file
    return;
  }

  // Compile the pixel shader code.
  result = D3DX11CompileFromFile(ps_path, NULL, NULL, ps_name, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, NULL, 
    &pixel_shader_buffer, &error_message, NULL);
  if(FAILED(result))
  {
    if(error_message) output_shader_errors(error_message, ps_path);
    else assert(0); // Could not find shader file
    return;
  }

  // Create the vertex shader from the buffer.
  result = device->CreateVertexShader(vertex_shader_buffer->GetBufferPointer(), vertex_shader_buffer->GetBufferSize(), NULL, &output_shader->vertex_shader);
  assert(!FAILED(result));

  // Create the pixel shader from the buffer.
  result = device->CreatePixelShader(pixel_shader_buffer->GetBufferPointer(), pixel_shader_buffer->GetBufferSize(), NULL, &output_shader->pixel_shader);
  assert(!FAILED(result));


  // Create the vertex input layout.
  result = device->CreateInputLayout(common_input_vertex_layout, num_elements, vertex_shader_buffer->GetBufferPointer(), 
    vertex_shader_buffer->GetBufferSize(), &output_shader->layout);
  assert(!FAILED(result));

  // Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
  vertex_shader_buffer->Release();
  pixel_shader_buffer->Release();



  output_shader->global_buffer = in_buffer;
}

static mat4 make_ortho_projection_matrix(float width, float aspect_ratio, float near_plane, float far_plane)
{
  float height = width / aspect_ratio;
  float l = -width / 2.0f;
  float r = width / 2.0f;
  float t = height / 2.0f;
  float b = -height / 2.0f;
  float n = near_plane;
  float f = far_plane;
  mat4 ortho =
  {
    2.0f / width, 0.0f, 0.0f, -(r + l) / (r - l),
    0.0f, 2.0f / height, 0.0f, -(t + b) / (t - b),
    0.0f, 0.0f, -1.0f / (f - n), n / (f - n),
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return ortho;
}


static void make_quad(Mesh::Vertex *vertices, unsigned *indices)
{
  vertices[0] = Mesh::Vertex(v3(0.0f, 0.0f, 0.0f), v2(0.0f, 1.0f)); // Left lower
  vertices[1] = Mesh::Vertex(v3(1.0f, 0.0f, 0.0f), v2(1.0f, 1.0f)); // Right lower
  vertices[2] = Mesh::Vertex(v3(1.0f, 1.0f, 0.0f), v2(1.0f, 0.0f)); // Right upper
  vertices[3] = Mesh::Vertex(v3(0.0f, 1.0f, 0.0f), v2(0.0f, 0.0f)); // Left upper

  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 2;
  indices[4] = 3;
  indices[5] = 0;
}

static void set_viewport(float width, float height, v2 offset)
{
  D3D11_VIEWPORT viewport = {};
  viewport.Width = width;
  viewport.Height = height;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  viewport.TopLeftX = offset.x;
  viewport.TopLeftY = offset.y;

  // Create the viewport.
  renderer_data->resources.device_context->RSSetViewports(1, &viewport);
}











void Mesh::fill_buffers(ID3D11Device *device)
{
  // Vertices
  {
    // Set up the description of the static vertex buffer.
    D3D11_BUFFER_DESC vertex_buffer_desc;
    vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    vertex_buffer_desc.ByteWidth = sizeof(Vertex) * vertices.size();
    vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vertex_buffer_desc.MiscFlags = 0;
    vertex_buffer_desc.StructureByteStride = 0;

    // Give the subresource structure a pointer to the vertex data.
    D3D11_SUBRESOURCE_DATA vertex_data;
    vertex_data.pSysMem = vertices.data();
    vertex_data.SysMemPitch = 0;
    vertex_data.SysMemSlicePitch = 0;

    // Now create the vertex buffer.
    HRESULT result = device->CreateBuffer(&vertex_buffer_desc, &vertex_data, &vertex_buffer);
    assert(!FAILED(result));
  }

  // Indices
  {
    D3D11_BUFFER_DESC index_buffer_desc;
    index_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    index_buffer_desc.ByteWidth = sizeof(unsigned) * indices.size();
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    index_buffer_desc.MiscFlags = 0;
    index_buffer_desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA index_data;
    index_data.pSysMem = indices.data();
    index_data.SysMemPitch = 0;
    index_data.SysMemSlicePitch = 0;

    HRESULT result = device->CreateBuffer(&index_buffer_desc, &index_data, &index_buffer);
    assert(!FAILED(result));
  }
}

void Mesh::update_data(ID3D11DeviceContext *device_context, void *data, unsigned num_bytes)
{
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  HRESULT result = device_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
  assert(!FAILED(result));

  memcpy(mapped_resource.pData, data, num_bytes);

  device_context->Unmap(vertex_buffer, 0);
}


void Mesh::clear_buffers()
{
  if(index_buffer) index_buffer->Release();
  if(vertex_buffer) vertex_buffer->Release();
}







void init_renderer(HWND window_handle, unsigned in_framebuffer_width, unsigned in_framebuffer_height, bool is_fullscreen, bool is_vsync)
{
  renderer_data = new RendererData();

  renderer_data->shader_errors_file = fopen("shader_errors.txt", "wt");


  renderer_data->window.handle = window_handle;
  renderer_data->window.fullscreen = is_fullscreen;
  renderer_data->window.vsync = is_vsync;
  renderer_data->window.framebuffer_width = in_framebuffer_width;
  renderer_data->window.framebuffer_height = in_framebuffer_height;

  HRESULT result;

  // All references to the renderer data's memory for easier coding :)))
  unsigned &video_card_memory_bytes = renderer_data->resources.video_card_memory_bytes;
  unsigned &framebuffer_width = renderer_data->window.framebuffer_width;
  unsigned &framebuffer_height = renderer_data->window.framebuffer_height;
  bool &vsync = renderer_data->window.vsync;
  bool &fullscreen = renderer_data->window.fullscreen;
  float &aspect_ratio = renderer_data->window.aspect_ratio;

  IDXGISwapChain *&swap_chain = renderer_data->resources.swap_chain;
  ID3D11Device *&device = renderer_data->resources.device;
  ID3D11DeviceContext *&device_context = renderer_data->resources.device_context;
  ID3D11RenderTargetView *&render_target_view = renderer_data->resources.render_target_view;
  ID3D11Texture2D *&depth_stencil_buffer = renderer_data->resources.depth_stencil_buffer;
  ID3D11DepthStencilState *&depth_stencil_state = renderer_data->resources.depth_stencil_state;
  ID3D11DepthStencilState *&no_depth_stencil_state = renderer_data->resources.no_depth_stencil_state;
  ID3D11DepthStencilView *&depth_stencil_view = renderer_data->resources.depth_stencil_view;
  ID3D11RasterizerState *&raster_state = renderer_data->resources.raster_state;
  ID3D11RasterizerState *&wireframe_raster_state = renderer_data->resources.wireframe_raster_state;

  ID3D11Texture2D *&render_target_texture = renderer_data->resources.render_target_texture;
  ID3D11RenderTargetView *&render_to_texture_target_view = renderer_data->resources.render_to_texture_target_view;
  ID3D11ShaderResourceView *&render_target_shader_resource_view = renderer_data->resources.render_target_shader_resource_view;


  // Create a DirectX graphics interface factory.
  IDXGIFactory *factory;
  result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&factory);
  assert(!FAILED(result));


  // Use the factory to create an adapter for the primary graphics interface (video card).
  IDXGIAdapter *adapter;
  result = factory->EnumAdapters(0, &adapter);
  assert(!FAILED(result));

  // Enumerate the primary adapter output (monitor).
  IDXGIOutput *adapter_output;
  result = adapter->EnumOutputs(0, &adapter_output);
  assert(!FAILED(result));

  // Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
  unsigned num_modes;
  result = adapter_output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &num_modes, NULL);
  assert(!FAILED(result));

  // Create a list to hold all the possible display modes for this monitor/video card combination.
  DXGI_MODE_DESC *display_mode_list = new DXGI_MODE_DESC[num_modes];

  // Now fill the display mode list structures.
  result = adapter_output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &num_modes, display_mode_list);
  assert(!FAILED(result));

  // Get the adapter (video card) description.
  DXGI_ADAPTER_DESC adapter_desc;
  result = adapter->GetDesc(&adapter_desc);
  assert(!FAILED(result));

  // Store the dedicated video card memory in megabytes.
  video_card_memory_bytes = (unsigned)adapter_desc.DedicatedVideoMemory;

  // Convert the name of the video card to a character array and store it.
  // TODO:
  // unsigned string_length;
  // error = wcstombs_s(&string_length, video_card_description, 128, adapter_desc.Description, 128);
  // assert(error == 0);

  // Release the display mode list.
  delete [] display_mode_list;
  display_mode_list = 0;

  // Release the adapter output.
  adapter_output->Release();
  adapter_output = 0;

  // Release the adapter.
  adapter->Release();
  adapter = 0;

  // Release the factory.
  factory->Release();
  factory = 0;



  // Set to a single back buffer.
  DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
  swap_chain_desc.BufferCount = 1;

  // Set the width and height of the back buffer.
  swap_chain_desc.BufferDesc.Width = framebuffer_width;
  swap_chain_desc.BufferDesc.Height = framebuffer_height;

  // Set regular 32-bit surface for the back buffer.
  swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

  // Set the refresh rate of the back buffer.
  if(vsync)
  {
    // VSYNC NOT SUPPORTED YET
    assert(0);

#if 0
    // Now go through all the display modes and find the one that matches the screen width and height.
    // When a match is found store the numerator and denominator of the refresh rate for that monitor.

    // THIS IS NOT CORRECT
    // There are multiple refresh rates for the same dimensions of the monitor and this will just pick the first refresh rate in that list.
    // There needs to be some better way to get the current refresh rate of the monitor.
    unsigned numerator;
    unsigned denominator;
    for(unsigned i = 0; i < num_modes; i++)
    {
      if(display_mode_list[i].Width == (unsigned)framebuffer_width)
      {
        if(display_mode_list[i].Height == (unsigned)framebuffer_height)
        {
          numerator = display_mode_list[i].RefreshRate.Numerator;
          denominator = display_mode_list[i].RefreshRate.Denominator;
        }
      }
    }

    swap_chain_desc.BufferDesc.RefreshRate.Numerator = numerator;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = denominator;
#endif
  }
  else
  {
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 0;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
  }

  // Set the usage of the back buffer.
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

  // Set the handle for the window to render to.
  swap_chain_desc.OutputWindow = window_handle;

  // Turn multisampling off.
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;

  // Set to full screen or windowed mode.
  if(fullscreen == true)
  {
    swap_chain_desc.Windowed = false;
  }
  else
  {
    swap_chain_desc.Windowed = true;
  }

  // Set the scan line ordering and scaling to unspecified.
  swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

  // Discard the back buffer contents after presenting.
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  // Advanced flags.
  swap_chain_desc.Flags = 0; // DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  // Set the feature level to DirectX 11.
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  // Create the swap chain, Direct3D device, and Direct3D device context.
  result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &feature_level, 1, 
                                         D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, NULL, &device_context
                                        );
  assert(!FAILED(result));

  // Get the pointer to the back buffer.
  ID3D11Texture2D *back_buffer;
  result = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&back_buffer);
  assert(!FAILED(result));

  // Create the render target view with the back buffer pointer.
  result = device->CreateRenderTargetView(back_buffer, NULL, &render_target_view);
  assert(!FAILED(result));

  // Release pointer to the back buffer as we no longer need it.
  back_buffer->Release();
  back_buffer = 0;

  // Set up the description of the depth buffer.
  D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
  depth_buffer_desc.Width = framebuffer_width;
  depth_buffer_desc.Height = framebuffer_height;
  depth_buffer_desc.MipLevels = 1;
  depth_buffer_desc.ArraySize = 1;
  depth_buffer_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_buffer_desc.SampleDesc.Count = 1;
  depth_buffer_desc.SampleDesc.Quality = 0;
  depth_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
  depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  depth_buffer_desc.CPUAccessFlags = 0;
  depth_buffer_desc.MiscFlags = 0;

  // Create the texture for the depth buffer using the filled out description.
  result = device->CreateTexture2D(&depth_buffer_desc, NULL, &depth_stencil_buffer);
  assert(!FAILED(result));

  // Set up the description of the stencil state.
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
  depth_stencil_desc.DepthEnable = false;
  depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;

  depth_stencil_desc.StencilEnable = true;
  depth_stencil_desc.StencilReadMask = 0xFF;
  depth_stencil_desc.StencilWriteMask = 0xFF;

  // Stencil operations if pixel is front-facing.
  depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
  depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

  // Stencil operations if pixel is back-facing.
  depth_stencil_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
  depth_stencil_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

  // Create the depth stencil state.
  result = device->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);
  assert(!FAILED(result));
  // Set the depth stencil state.
  device_context->OMSetDepthStencilState(depth_stencil_state, 1);


  D3D11_DEPTH_STENCIL_DESC no_depth_stencil_desc = {};
  result = device->CreateDepthStencilState(&no_depth_stencil_desc, &no_depth_stencil_state);
  assert(!FAILED(result));


  // Set up the depth stencil view description.
  D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {};
  depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  depth_stencil_view_desc.Texture2D.MipSlice = 0;

  // Create the depth stencil view.
  result = device->CreateDepthStencilView(depth_stencil_buffer, &depth_stencil_view_desc, &depth_stencil_view);
  assert(!FAILED(result));

  // Bind the render target view and depth stencil buffer to the output render pipeline.
  device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

  // Setup the raster description which will determine how and what polygons will be drawn.
  D3D11_RASTERIZER_DESC raster_desc = {};
  raster_desc.AntialiasedLineEnable = false;
  raster_desc.CullMode = D3D11_CULL_BACK;
  raster_desc.DepthBias = 0;
  raster_desc.DepthBiasClamp = 0.0f;
  raster_desc.DepthClipEnable = true;
  raster_desc.FillMode = D3D11_FILL_SOLID;
  //raster_desc.FillMode = D3D11_FILL_WIREFRAME;
  raster_desc.FrontCounterClockwise = true;
  raster_desc.MultisampleEnable = false;
  raster_desc.ScissorEnable = false;
  raster_desc.SlopeScaledDepthBias = 0.0f;

  // Create the rasterizer state from the description we just filled out.
  result = device->CreateRasterizerState(&raster_desc, &raster_state);
  assert(!FAILED(result));

  raster_desc.FillMode = D3D11_FILL_WIREFRAME;
  result = device->CreateRasterizerState(&raster_desc, &wireframe_raster_state);
  assert(!FAILED(result));

  // Now set the rasterizer state.
  device_context->RSSetState(raster_state);

  // Setup the viewport for rendering.
  set_viewport(framebuffer_width, framebuffer_height, v2());

  aspect_ratio = (float)framebuffer_width / (float)framebuffer_height;


  // Blend state
  ID3D11BlendState *blendState = NULL;
  D3D11_BLEND_DESC blendStateDesc = {};
  
  blendStateDesc.RenderTarget[0].BlendEnable = TRUE;
  blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0x0f;

  device->CreateBlendState(&blendStateDesc, &blendState);

  float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  device_context->OMSetBlendState(blendState, blendFactor, 0xffffffff);







  // Shaders


  // As of 7/25/2019 all vertices in meshes have the same vertex layout in an AOS format.
  // So, each shader uses the same element description for input vertices.
  // This input element descrtion would need to change if different shaders took in a
  // different vertex layout.
  D3D11_INPUT_ELEMENT_DESC common_input_vertex_layout[2];
  common_input_vertex_layout[0].SemanticName = "POSITION";
  common_input_vertex_layout[0].SemanticIndex = 0;
  common_input_vertex_layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  common_input_vertex_layout[0].InputSlot = 0;
  common_input_vertex_layout[0].AlignedByteOffset = 0;
  common_input_vertex_layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  common_input_vertex_layout[0].InstanceDataStepRate = 0;

  common_input_vertex_layout[1].SemanticName = "TEXCOORD";
  common_input_vertex_layout[1].SemanticIndex = 0;
  common_input_vertex_layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  common_input_vertex_layout[1].InputSlot = 0;
  common_input_vertex_layout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  common_input_vertex_layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  common_input_vertex_layout[1].InstanceDataStepRate = 0;







  //
  // TODO:
  // Making a new global buffer for each shader to hold matrices is redundant.
  // Maybe there are new buffers I can make and attach them on top of the matrix buffer when I need to extend the shaders?
  // For now I'll just have the new shaders use the first matrix buffer.
  D3D11_BUFFER_DESC matrix_buffer_desc;
  matrix_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  matrix_buffer_desc.ByteWidth = sizeof(FirstShaderBuffer);
  matrix_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  matrix_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  matrix_buffer_desc.MiscFlags = 0;
  matrix_buffer_desc.StructureByteStride = 0;
  result = device->CreateBuffer(&matrix_buffer_desc, NULL, &renderer_data->first_shader_buffer);
  assert(!FAILED(result));


  unsigned num_elements = sizeof(common_input_vertex_layout) / sizeof(common_input_vertex_layout[0]);
  create_shader("shaders/quad.vs", "quad_vertex_shader", "shaders/quad.ps", "quad_pixel_shader", common_input_vertex_layout,
                num_elements, &renderer_data->quad_shader, renderer_data->first_shader_buffer);



  // Make primitive quad
  renderer_data->quad_mesh.vertices.resize(4);
  renderer_data->quad_mesh.indices.resize(6);
  make_quad(renderer_data->quad_mesh.vertices.data(), renderer_data->quad_mesh.indices.data());
  renderer_data->quad_mesh.fill_buffers(device);



  // Render to texture setup
  {
    // Setup the render target texture description.
    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = renderer_data->window.framebuffer_width;
    texture_desc.Height = renderer_data->window.framebuffer_height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // Why is this float??
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    D3D11_SAMPLER_DESC samplerDesc;
    // Create a texture sampler state description.
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    //samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.BorderColor[0] = 0;
    samplerDesc.BorderColor[1] = 0;
    samplerDesc.BorderColor[2] = 0;
    samplerDesc.BorderColor[3] = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    // Create the texture sampler state.
    HRESULT result = renderer_data->resources.device->CreateSamplerState(&samplerDesc, &renderer_data->quad_texture.sample_state);
    assert(!FAILED(result));

    // Create the render target texture.
    result = device->CreateTexture2D(&texture_desc, NULL, &render_target_texture);
    assert(!FAILED(result));

    // Setup the description of the render target view.
    D3D11_RENDER_TARGET_VIEW_DESC view_desc = {};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipSlice = 0;

    // Create the render target view.
    result = device->CreateRenderTargetView(render_target_texture, &view_desc, &render_to_texture_target_view);
    assert(!FAILED(result));

    // Setup the description of the shader resource view.
    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_viewDesc = {};
    shader_resource_viewDesc.Format = texture_desc.Format;
    shader_resource_viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_viewDesc.Texture2D.MostDetailedMip = 0;
    shader_resource_viewDesc.Texture2D.MipLevels = 1;

    // Create the shader resource view.
    result = device->CreateShaderResourceView(render_target_texture, &shader_resource_viewDesc, &render_target_shader_resource_view);
    assert(!FAILED(result));


    renderer_data->quad_texture.resource = render_target_shader_resource_view;
  }



  renderer_data->grid_pixels_height = renderer_data->window.framebuffer_height;
  renderer_data->grid_pixels_width = (10.0f / 24.0f) * renderer_data->grid_pixels_height;
  renderer_data->bar_width = (renderer_data->window.framebuffer_width - renderer_data->grid_pixels_width) / 2.0f;
}

/*
static void render_mesh(Mesh *mesh, Camera *camera, Shader *shader, v3 position, v2 scale, float rotation, v4 color,
                        Texture *texture, D3D_PRIMITIVE_TOPOLOGY topology, bool wireframe = false)
{
  ID3D11DeviceContext *device_context = renderer_data->resources.device_context;
  ID3D11RasterizerState *raster_state = renderer_data->resources.raster_state;
  ID3D11RasterizerState *wireframe_raster_state = renderer_data->resources.wireframe_raster_state;
  Window *window = &renderer_data->window;


  // Setting draw state
  if(wireframe) raster_state = wireframe_raster_state;
  device_context->RSSetState(raster_state);


  // Vertex buffers
  ID3D11Buffer *buffers[] = {mesh->vertex_buffer};
  unsigned strides[] = {sizeof(Mesh::Vertex)};
  unsigned offsets[] = {0};
  unsigned num_buffers = sizeof(buffers) / sizeof(buffers[0]);
  device_context->IASetVertexBuffers(0, num_buffers, buffers, strides, offsets);
  device_context->IASetIndexBuffer(mesh->index_buffer, DXGI_FORMAT_R32_UINT, 0);
  device_context->IASetPrimitiveTopology(topology);


  // Matrices
  mat4 world_m_model = make_world_matrix(position, scale, rotation);
  mat4 view_m_world = make_view_matrix(camera);
  mat4 clip_m_view = make_clip_matrix(camera);

  // Shaders
  device_context->IASetInputLayout(shader->layout);
  device_context->VSSetShader(shader->vertex_shader, NULL, 0);
  device_context->PSSetShader(shader->pixel_shader, NULL, 0);


  // Global shader buffers
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
  assert(!FAILED(result));

  FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
  data->world_m_model = world_m_model;
  data->view_m_world = view_m_world;
  data->clip_m_view = clip_m_view;
  data->color = color;
  device_context->Unmap(shader->global_buffer, 0);

  device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);

  // Textures
  if(texture)
  {
    device_context->PSSetShaderResources(0, 1, &texture->resource);

    device_context->PSSetSamplers(0, 1, &texture->sample_state);
  }



  // Render
  unsigned num_indices = mesh->indices.size();
  device_context->DrawIndexed(num_indices, 0, 0);
}
*/


static void render_grid()
{
  ID3D11DeviceContext *device_context = renderer_data->resources.device_context;
  ID3D11RasterizerState *raster_state = renderer_data->resources.raster_state;
  ID3D11RasterizerState *wireframe_raster_state = renderer_data->resources.wireframe_raster_state;


  Mesh *mesh = &renderer_data->quad_mesh;
  Shader *shader = &renderer_data->quad_shader;

  // Vertex buffers
  ID3D11Buffer *buffers[] = {mesh->vertex_buffer};
  unsigned strides[] = {sizeof(Mesh::Vertex)};
  unsigned offsets[] = {0};
  unsigned num_buffers = sizeof(buffers) / sizeof(buffers[0]);
  device_context->IASetVertexBuffers(0, num_buffers, buffers, strides, offsets);
  device_context->IASetIndexBuffer(mesh->index_buffer, DXGI_FORMAT_R32_UINT, 0);
  device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



  // Shaders
  device_context->IASetInputLayout(shader->layout);
  device_context->VSSetShader(shader->vertex_shader, NULL, 0);
  device_context->PSSetShader(shader->pixel_shader, NULL, 0);



  // Textures
  /*
  if(texture)
  {
    device_context->PSSetShaderResources(0, 1, &texture->resource);

    device_context->PSSetSamplers(0, 1, &texture->sample_state);
  }
  */

  const float thickness = 0.05f;
  const v4 separator_color = v4(0.0f, 0.0f, 0.0f, 1.0f);



  for(unsigned i = 0; i < renderer_data->cells_to_render.size(); i++)
  {
    CellData *cell = &renderer_data->cells_to_render[i];

    // Matrices
    mat4 world_m_model =
    {
      1.0f, 0.0f, 0.0f, (float)cell->position.x,
      0.0f, 1.0f, 0.0f, (float)cell->position.y,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };
    const float GRID_WIDTH = 10.0f;
    const float GRID_HEIGHT = 24.0f;
    mat4 clip_m_view =
    {
      2.0f / GRID_WIDTH, 0.0f,  0.0f, -1.0f,
      0.0f, 2.0f / GRID_HEIGHT, 0.0f, -1.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    // Global shader buffers
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    assert(!FAILED(result));

    FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
    data->world_m_model = world_m_model;
    data->view_m_world = mat4();
    data->clip_m_view = clip_m_view;
    data->color = v4(cell->color.r, cell->color.g, cell->color.b, cell->color.a);
    device_context->Unmap(shader->global_buffer, 0);

    device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);


    // Render
    unsigned num_indices = mesh->indices.size();
    device_context->DrawIndexed(num_indices, 0, 0);
  }

  // Row separators
  for(unsigned i = 0; i < 24; i++)
  {
    float y = (float)i;

    // Matrices
    mat4 world_m_model =
    {
      GRID_WIDTH, 0.0f, 0.0f, 0.0f,
      0.0f, thickness, 0.0f, y,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };
    mat4 clip_m_view =
    {
      2.0f / GRID_WIDTH, 0.0f,  0.0f, -1.0f,
      0.0f, 2.0f / GRID_HEIGHT, 0.0f, -1.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    // Global shader buffers
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    assert(!FAILED(result));

    FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
    data->world_m_model = world_m_model;
    data->view_m_world = mat4();
    data->clip_m_view = clip_m_view;
    data->color = separator_color;
    device_context->Unmap(shader->global_buffer, 0);

    device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);


    // Render
    unsigned num_indices = mesh->indices.size();
    device_context->DrawIndexed(num_indices, 0, 0);
  }

  // Column separators
  for(unsigned i = 0; i < 10; i++)
  {
    float x = (float)i;

    // Matrices
    mat4 world_m_model =
    {
      thickness, 0.0f, 0.0f, x,
      0.0f, GRID_HEIGHT, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };
    mat4 clip_m_view =
    {
      2.0f / GRID_WIDTH, 0.0f,  0.0f, -1.0f,
      0.0f, 2.0f / GRID_HEIGHT, 0.0f, -1.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    // Global shader buffers
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    assert(!FAILED(result));

    FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
    data->world_m_model = world_m_model;
    data->view_m_world = mat4();
    data->clip_m_view = clip_m_view;
    data->color = separator_color;
    device_context->Unmap(shader->global_buffer, 0);

    device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);


    // Render
    unsigned num_indices = mesh->indices.size();
    device_context->DrawIndexed(num_indices, 0, 0);
  }


  renderer_data->cells_to_render.clear();
}

static void render_bar(std::vector<CellData> *cells_to_render, float bar_width, float bar_height)
{
  ID3D11DeviceContext *device_context = renderer_data->resources.device_context;
  ID3D11RasterizerState *raster_state = renderer_data->resources.raster_state;
  ID3D11RasterizerState *wireframe_raster_state = renderer_data->resources.wireframe_raster_state;


  Mesh *mesh = &renderer_data->quad_mesh;
  Shader *shader = &renderer_data->quad_shader;

  // Vertex buffers
  ID3D11Buffer *buffers[] = {mesh->vertex_buffer};
  unsigned strides[] = {sizeof(Mesh::Vertex)};
  unsigned offsets[] = {0};
  unsigned num_buffers = sizeof(buffers) / sizeof(buffers[0]);
  device_context->IASetVertexBuffers(0, num_buffers, buffers, strides, offsets);
  device_context->IASetIndexBuffer(mesh->index_buffer, DXGI_FORMAT_R32_UINT, 0);
  device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



  // Shaders
  device_context->IASetInputLayout(shader->layout);
  device_context->VSSetShader(shader->vertex_shader, NULL, 0);
  device_context->PSSetShader(shader->pixel_shader, NULL, 0);



  // Textures
  /*
  if(texture)
  {
    device_context->PSSetShaderResources(0, 1, &texture->resource);

    device_context->PSSetSamplers(0, 1, &texture->sample_state);
  }
  */


  // Render background
  {
    // Global shader buffers
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    assert(!FAILED(result));

    FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
    data->world_m_model = mat4(2.0f, 0.0f, 0.0f, -1.0f,
                               0.0f, 2.0f, 0.0f, -1.0f,
                               0.0f, 0.0f, 1.0f,  0.0f,
                               0.0f, 0.0f, 0.0f,  1.0f
                              );
    data->view_m_world = mat4();
    data->clip_m_view = mat4();
    data->color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    device_context->Unmap(shader->global_buffer, 0);

    device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);


    // Render
    unsigned num_indices = mesh->indices.size();
    device_context->DrawIndexed(num_indices, 0, 0);
  }


  for(unsigned i = 0; i < cells_to_render->size(); i++)
  {
    CellData *cell = &(*cells_to_render)[i];

    // Matrices
    mat4 world_m_model =
    {
      1.0f, 0.0f, 0.0f, (float)cell->position.x,
      0.0f, 1.0f, 0.0f, (float)cell->position.y,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };
    float ratio = bar_width / bar_height;
    mat4 clip_m_view =
    {
      1.0f / 3.0f, 0.0f,  0.0f, -0.33f,
      0.0f, ratio / 3.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    // Global shader buffers
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT result = device_context->Map(shader->global_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    assert(!FAILED(result));

    FirstShaderBuffer *data = (FirstShaderBuffer *)mapped_resource.pData;
    data->world_m_model = world_m_model;
    data->view_m_world = mat4();
    data->clip_m_view = clip_m_view;
    data->color = v4(cell->color.r, cell->color.g, cell->color.b, cell->color.a);
    device_context->Unmap(shader->global_buffer, 0);

    device_context->VSSetConstantBuffers(0, 1, &shader->global_buffer);


    // Render
    unsigned num_indices = mesh->indices.size();
    device_context->DrawIndexed(num_indices, 0, 0);
  }


  cells_to_render->clear();
}

void render()
{
  D3DResources *resources = &renderer_data->resources;

  // Clear the back and depth buffer.
  resources->device_context->OMSetRenderTargets(1, &resources->render_target_view, resources->depth_stencil_view);
  resources->device_context->ClearRenderTargetView(resources->render_target_view, renderer_data->window.background_color);
  resources->device_context->ClearDepthStencilView(resources->depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0);

  Window *window = &renderer_data->window;

  float grid_pixels_height = renderer_data->grid_pixels_height;
  float grid_pixels_width = renderer_data->grid_pixels_width;
  float bar_width = renderer_data->bar_width;

  set_viewport(bar_width, grid_pixels_height, v2());
  render_bar(&renderer_data->left_bar_cells_to_render, bar_width, grid_pixels_height);


  set_viewport(grid_pixels_width, grid_pixels_height, v2(bar_width, 0.0f));
  render_grid();


  set_viewport(bar_width, grid_pixels_height, v2(bar_width + grid_pixels_width, 0.0f));
  render_bar(&renderer_data->right_bar_cells_to_render, bar_width, grid_pixels_height);
}

void swap_frame()
{
  D3DResources *resources = &renderer_data->resources;

  // Present the back buffer to the screen since rendering is complete.
  if(renderer_data->window.vsync)
  {
    // Lock to screen refresh rate.
    resources->swap_chain->Present(1, 0);
  }
  else
  {
    // Present as fast as possible.
    resources->swap_chain->Present(0, 0);
  }
}

void shutdown_renderer()
{
  fclose(renderer_data->shader_errors_file);

  // Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
  if(renderer_data->resources.swap_chain)
  {
    renderer_data->resources.swap_chain->SetFullscreenState(false, NULL);
  }

  if(renderer_data->resources.raster_state)
  {
    renderer_data->resources.raster_state->Release();
  }

  if(renderer_data->resources.depth_stencil_view)
  {
    renderer_data->resources.depth_stencil_view->Release();
  }

  if(renderer_data->resources.depth_stencil_state)
  {
    renderer_data->resources.depth_stencil_state->Release();
  }

  if(renderer_data->resources.depth_stencil_buffer)
  {
    renderer_data->resources.depth_stencil_buffer->Release();
  }

  if(renderer_data->resources.render_target_view)
  {
    renderer_data->resources.render_target_view->Release();
  }

  if(renderer_data->resources.device_context)
  {
    renderer_data->resources.device_context->Release();
  }

  if(renderer_data->resources.device)
  {
    renderer_data->resources.device->Release();
  }

  if(renderer_data->resources.swap_chain)
  {
    renderer_data->resources.swap_chain->Release();
  }

#if 0

  // Release the matrix constant buffer.
  if(matrix_buffer)
  {
    matrix_buffer->Release();
  }

  // Release the layout.
  if(layout)
  {
    layout->Release();
  }

  // Release the pixel shader.
  if(pixel_shader)
  {
    pixel_shader->Release();
  }

  // Release the vertex shader.
  if(vertex_shader)
  {
    vertex_shader->Release();
  }

  // Release the texture resource.
  for(int i = 0; i < sizeof(textures) / sizeof(textures[0]); i++)
  {
    if(textures[i])
    {
      textures[i]->Release();
    }
  }
#endif
}

void draw_cell(v2i position, Color color)
{
  CellData cell = {position, color};
  renderer_data->cells_to_render.push_back(cell);
}

void draw_cell_in_left_bar(v2i position, Color color)
{
  CellData cell = {position, color};
  renderer_data->left_bar_cells_to_render.push_back(cell);
}

void draw_cell_in_right_bar(v2i position, Color color)
{
  CellData cell = {position, color};
  renderer_data->right_bar_cells_to_render.push_back(cell);
}

v2 window_to_world_space(v2 window_position)
{
  Window *window = &renderer_data->window;
  v2 window_middle_pos = window_position;
  window_middle_pos.x -= renderer_data->bar_width;

  if(window_middle_pos.x < 0.0f) window_middle_pos.x = 0.0f;
  if(window_middle_pos.x >= renderer_data->grid_pixels_width) window_middle_pos.x = renderer_data->grid_pixels_width - 1;
  if(window_middle_pos.y < 0.0f) window_middle_pos.y = 0.0f;
  if(window_middle_pos.y >= renderer_data->grid_pixels_height) window_middle_pos.y = renderer_data->grid_pixels_height - 1;
  
  v2 ndc_pos = window_middle_pos;
  ndc_pos.x /= renderer_data->grid_pixels_width;
  ndc_pos.y /= renderer_data->grid_pixels_height;
  ndc_pos.y = 1.0f - ndc_pos.y;

  v2 world_pos = ndc_pos;
  world_pos.x *= GRID_WIDTH;
  world_pos.y *= GRID_HEIGHT;

  return world_pos;
}

ID3D11Device *get_d3d_device() { return renderer_data->resources.device; }
ID3D11DeviceContext *get_d3d_device_context() { return renderer_data->resources.device_context; }

