
#include "network_client.h"

#include <WinSock2.h> // Networking API
#include <Ws2tcpip.h> // InetPton

#include <cstdio>

struct NetworkData
{
  SOCKET udp_socket;
  sockaddr_in address;

  unsigned grid_width;
  unsigned grid_height;
  unsigned *grid;
};

static NetworkData *network_data;
static const unsigned MAX_BRIGHTNESS_VALUE = 10;





static void init_winsock()
{
  // Initialize Winsock
  WSADATA wsa_data;
  int error = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if(error) fprintf(stderr, "Error loading networking module: %i\n", WSAGetLastError());
}

static void shutdown_winsock()
{
  // Shut down Winsock
  int error = WSACleanup();
  if(error == -1)
  {
    error = WSAGetLastError();
    fprintf(stderr, "Error unloading networking module: %i\n", error);
    return;
  }
}

static SOCKET create_udp_socket()
{
  // Create a socket
  SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if(udp_socket == INVALID_SOCKET)
  {
    int error = WSAGetLastError();
    fprintf(stderr, "Error opening socket: %i\n", error);
  }

  return udp_socket;
}

void make_address(const char *ip_address_string, int port_number, sockaddr_in *out_address)
{
  // Create a remote address
  out_address->sin_family = AF_INET;
  out_address->sin_port = htons(port_number);
  int error = InetPton(AF_INET, ip_address_string, &(out_address->sin_addr));
  if((error != 0) && (error != 1))
  {
    error = WSAGetLastError();
    fprintf(stderr, "Error creating an address: %i\n", error);
  }
}

void send_data(SOCKET socket, const void *data, unsigned bytes, const sockaddr_in *address)
{
  // Send data over UDP
  int bytes_queued;
  bytes_queued = sendto(socket, (const char *)data, bytes, 0, (const sockaddr *)(address), sizeof(*address));
  if(bytes_queued == -1)
  {
    int error = WSAGetLastError();
    fprintf(stderr, "Error sending data: %i\n", error);
  }
}

void close_socket(SOCKET socket)
{
  // Close the socket
  int error = closesocket(socket);
  if(error == -1)
  {
    error = WSAGetLastError();
    fprintf(stderr, "Error closing socket: %i\n", error);
  }
}






void init_network_client(const char *ip_address, int port, unsigned width, unsigned height)
{
  init_winsock();

  network_data = (NetworkData *)malloc(sizeof(NetworkData));
  network_data->grid_width = width;
  network_data->grid_height = height;

  int bytes = sizeof(unsigned) * network_data->grid_width * network_data->grid_height; 
  network_data->grid =(unsigned *)malloc(bytes);

  memset(network_data->grid, 0, bytes);

  network_data->udp_socket = create_udp_socket();
  make_address(ip_address, port, &(network_data->address));
}

void send_network_data()
{
  int bytes = sizeof(unsigned) * network_data->grid_width * network_data->grid_height;
  send_data(network_data->udp_socket,
            network_data->grid,
            bytes,
            &(network_data->address));

  memset(network_data->grid, 0, bytes);
}

void shutdown_network_client()
{
  // Send empty frame
  int bytes = sizeof(unsigned) * network_data->grid_width * network_data->grid_height;
  memset(network_data->grid, 0, bytes);
  send_network_data();


  free(network_data->grid);
  free(network_data);

  close_socket(network_data->udp_socket);
  shutdown_winsock();
}




void network_add_cell(v2i position, Color color)
{
  if(position.x < 0) return;
  if(position.y < 0) return;
  if(position.x >= network_data->grid_width)  return;
  if(position.y >= network_data->grid_height) return;

  if(position.y % 2 == 1) position.x = (network_data->grid_width - 1) - position.x;


  float alpha = color.a;
  unsigned r = MAX_BRIGHTNESS_VALUE * color.r * alpha;
  unsigned g = MAX_BRIGHTNESS_VALUE * color.g * alpha;
  unsigned b = MAX_BRIGHTNESS_VALUE * color.b * alpha;
  unsigned value = (b << 16) | (g << 8) | (r << 0);


  (network_data->grid)[position.y * network_data->grid_width + position.x] = value;
}

void network_add_cell_in_left_bar(v2i position, Color color)
{
}

void network_add_cell_in_right_bar(v2i position, Color color)
{
}

