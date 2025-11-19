#define _CRT_SECURE_NO_WARNINGS
#include <enet/enet.h>
#include <stdio.h>
#include <cstring>

int main() {
    if (enet_initialize() != 0) {
        printf("Failed to initialize ENet.\n");
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 27015;

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        printf("Failed to create ENet server.\n");
        return 1;
    }

    printf("SantaNET initialized.\n");

    ENetEvent event;
    while (1) {
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {

            case ENET_EVENT_TYPE_CONNECT:
                printf("Client connected.\n");
                break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                char buf[256];
                strcpy(buf, (char*)event.packet->data);

                //printf("Received: %s\n", buf);

                // handle POS message
                if (strncmp(buf, "POS", 3) == 0)
                {
                    float x, y, z, yaw;
                    unsigned id;
                    char name[64];

                    sscanf(buf, "POS %f %f %f %f %u %s", &x, &y, &z, &yaw, &id, name);

                    //printf("POS from %u: %.2f %.2f %.2f yaw=%.2f %s\n", id, x, y, z, yaw, name);

                    char syncMsg[256];
                    sprintf(syncMsg, "SYNC %u %f %f %f %f %s", id, x, y, z, yaw, name);

                    // send to all OTHER clients
                    for (size_t i = 0; i < server->peerCount; i++)
                    {
                        ENetPeer* p = &server->peers[i];

                        if (p->state == ENET_PEER_STATE_CONNECTED && p != event.peer)
                        {
                            ENetPacket* pkt = enet_packet_create(
                                syncMsg, strlen(syncMsg) + 1,
                                ENET_PACKET_FLAG_UNSEQUENCED
                            );
                            enet_peer_send(p, 0, pkt);
                        }
                    }
                }

                enet_packet_destroy(event.packet);
                break;
            }




            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Client disconnected.\n");
                break;
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
