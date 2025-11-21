#include "enet_client.h"
#include "steam.h"

namespace net {

    ENetHost* g_client = nullptr;
    ENetPeer* g_serverPeer = nullptr;
    bool g_isConnected = false;


    bool ENet_Connect(const char* ip, int port)
    {
        if (enet_initialize() != 0) {
            std::cout << "[ENet] Failed to initialize\n";
            return false;
        }

        g_client = enet_host_create(NULL, 1, 2, 0, 0);
        if (!g_client) {
            std::cout << "[ENet] Failed to create ENet client host\n";
            return false;
        }

        ENetAddress address;
        enet_address_set_host(&address, ip);
        address.port = port;

        g_serverPeer = enet_host_connect(g_client, &address, 2, 0);
        if (!g_serverPeer) {
            std::cout << "[ENet] No available peers for connection\n";
            return false;
        }

        ENetEvent event;
        if (enet_host_service(g_client, &event, 5000) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            std::cout << "[ENet] Connected to " << ip << ":" << port << "\n";
            g_isConnected = true;
            return true;
        }

        std::cout << "[ENet] Failed to connect\n";
        enet_peer_reset(g_serverPeer);
        return false;
    }

    void ENet_Send(const std::string& msg)
    {
        if (!g_isConnected) return;

        ENetPacket* packet = enet_packet_create(
            msg.c_str(), msg.size() + 1,
            ENET_PACKET_FLAG_RELIABLE
        );

        enet_peer_send(g_serverPeer, 0, packet);
    }

    void ENet_Poll()
    {
        if (!g_client) return;

        ENetEvent event;
        while (enet_host_service(g_client, &event, 0) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_RECEIVE:
            {
                std::string msg((char*)event.packet->data);

                // Example: SYNC 1234 1.0 2.0 3.0 1.57
                if (msg.rfind("SYNC", 0) == 0)
                {
                    unsigned id;
                    float x, y, z, yaw;
                    char name[64];

                    sscanf(msg.c_str(), "SYNC %u %f %f %f %f %s", &id, &x, &y, &z, &yaw, name);
                    // ignore our own data
                    if (id != GetCurrentProcessId()) {
                        g_otherPlayers[id] = { x, y, z, yaw };
                        g_otherPlayers[id].name = name;
                    }
                }

                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "[ENet] Disconnected\n";
                g_isConnected = false;
                break;
            }
        }
    }

    void ENet_UpdatePlayerPos()
    {
        if (g_isConnected) {
            char buf[128];
            snprintf(buf, sizeof(buf), "POS %.3f %.3f %.3f %.3f %u %s",
                g_localPlayerPos.x, g_localPlayerPos.y, g_localPlayerPos.z,
                g_localPlayerYaw, GetCurrentProcessId(), g_steamName.c_str());

            ENet_Send(buf);
        }
    }

    void ENet_Disconnect() {
        if (!g_isConnected) return;
        enet_peer_disconnect(g_serverPeer, 0);
        enet_host_destroy(g_client);
        enet_deinitialize();
        g_isConnected = false;
    }
}