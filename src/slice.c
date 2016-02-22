/*
 * Slice: Reliable UDP server, it's a part of Shuttle project.
 * Created initially by Jack Waller on 2016-02-18.
 *
 * https://github.com/nxtreaming
 *
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <enet/enet.h>
#include "config.h"

int main (int argc, char *argv[])
{
    ENetAddress address;
    ENetHost *server;
    ENetPacket *packet;
    ENetEvent event;
    char host_ip[32] = {0};
    char peer_info[32] = {0};
    char msg_notify[64] = {0};
    char message[1024] = {0};
    int lastEvent = -1;
    int enableDebug = 0;
    int i;

    /* Initialize the ENet */
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error (%s) occured while initializing ENet.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    /* Create the server */
    address.host = ENET_HOST_ANY;
    address.port = SHUTTLE_SERVER_PORT;
    server = enet_host_create(&address, SHUTTLE_MAX_CLIENT_NUM, SHUTTLE_MAX_CHANNEL_NUM,
                              SHUTTLE_MAX_INCOMING_BANDWIDTH, SHUTTLE_MAX_OUTGOING_BANDWIDTH);
    if (server == NULL) {
        fprintf(stderr, "An error (%s) occured while trying to create ENet server host.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("(Slice) Server is running.\n");
    while (1) {
        /* Event dispatcher: MUST not be hanged up. */
        int eventStatus = enet_host_service(server, &event, 10);
        if (eventStatus >= 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_NONE:
                /* Silence huge repeated NONE events. */
                if (lastEvent != ENET_EVENT_TYPE_NONE) {
                    if (enableDebug)
                        printf("(Slice) No event.\n");
                }
                break;
            case ENET_EVENT_TYPE_CONNECT:
                /* Store any relevant server information here. */
                enet_address_get_host_ip(&event.peer->address, host_ip, sizeof(host_ip) - 1);
                snprintf(peer_info, sizeof(peer_info), "[%s:%d]", host_ip, event.peer->address.port);
                if (event.peer->data)
                    free(event.peer->data);
                event.peer->data = malloc(strlen(peer_info) + 1);
                if (event.peer->data)
                    strcpy(event.peer->data, peer_info);
                /* Notify other chaters. */
                snprintf(msg_notify, sizeof(msg_notify), "%s has connected to server", peer_info);
                /* enet_host_broadcast(server, 1, packet); */
                for (i=0; i < server->peerCount; i++) {
                    ENetPeer *peer = server->peers + i;

                    /* Notify other chaters. */
                    if (peer != event.peer) {
                        packet = enet_packet_create(msg_notify, strlen(msg_notify) + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 1, packet);
                        enet_host_flush(server);
                    }
                }
                printf("(Slice) Got a connection from peer: %s:%d.\n", host_ip, event.peer->address.port);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                snprintf(message, sizeof(message), "%s:%s", (char*)event.peer->data, (char*)event.packet->data);
                for (i=0; i < server->peerCount; i++) {
                    ENetPeer *peer = server->peers + i;

                    /* Relay the message to other chaters. */
                    if (peer != event.peer) {
                        packet = enet_packet_create(message, strlen(message) + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 0, packet);
                        enet_host_flush(server);
                    }
                }
                printf("(Slice) Got a message from peer %s : %s\n", (char*)event.peer->data,
                       (char*)event.packet->data);
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                /* A connected peer has either explicitly disconnected or timed out. */
                if (event.peer->data) {
                    printf("(Slice) %s is disconnected.\n", (char*)event.peer->data);
                    free(event.peer->data);
                } else {
                    enet_address_get_host_ip(&event.peer->address, host_ip, sizeof(host_ip) - 1);
                    snprintf(peer_info, sizeof(peer_info), "[%s:%d]", host_ip, event.peer->address.port);
                    printf("(Slice) Unknown (%s) connection is disconnected.\n", peer_info);
                }
                /* Reset the peer's information. */
                event.peer->data = NULL;
                enet_peer_reset(event.peer);
                lastEvent = -1;
                break;
            default:
                assert(0);
                break;
            }

            lastEvent = event.type;
        } else {
            fprintf(stderr, "(Slice) Something went wrong: %d.\n", eventStatus);
            lastEvent = -1;
        }
    }

    enet_host_destroy(server);
    printf("Server is terminated.\n");

    return 0;
}
