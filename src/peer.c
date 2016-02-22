/*
 * Peer: Reliable UDP client, it's a part of Shuttle project.
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
#include <pthread.h>
#include <enet/enet.h>
#include "config.h"

typedef struct ChatContext {
    pthread_t thread;
    ENetHost *client;
    ENetPeer *peer;
    volatile int connected;
    volatile int terminated;
} ChatContext;

static void *peer_chater(void *handle)
{
    ChatContext *pcc = (ChatContext *)handle;
    ENetPacket *packet;
    char message[1024] = {0};
    int msgLen, ret, trycount;

    while (!pcc->terminated) {
        printf("Say >");
        message[0] = '\0';
        fgets(message, sizeof(message) - 1, stdin);
        msgLen = strlen(message);

        /* TODO: use synchronization object */
        /* Avoid message lost */
        while (!pcc->connected) {
            trycount = 0;
            while (trycount < 20 && !pcc->terminated) {
                sleep(50);
                trycount++;
                if (pcc->connected)
                    break;
            }
            if (pcc->terminated)
                break;
            if (!pcc->connected)
                printf("(Peer) Fail to connect to server.\n");
        }
        if (pcc->terminated) {
            printf("(Peer) Terminated the chat.\n");
            break;
        }

        /* We escape the only one '\n' case. */
        if (msgLen > 1) {
            /* We exclude the tail '\n'. */
            message[msgLen - 1] = '\0';
            packet = enet_packet_create(message, msgLen - 1, ENET_PACKET_FLAG_RELIABLE);
            ret = enet_peer_send(pcc->peer, 1, packet);
            if (ret < 0) {
                printf("(Peer) Fail to send packet: %d.\n", ret);
                enet_packet_destroy(packet);
                continue;
            }
            /* Send the packet ASAP. */
            enet_host_flush(pcc->client);
        }
    }

    printf("(Peer) Chater is terminated.\n");

    return 0;
}

int main (int argc, char *argv[])
{
    ENetAddress address;
    ENetEvent event;
    ChatContext cc, *pcc = &cc;
    char host_ip[32] = {0};
    char peer_info[32] = {0};
    int lastEvent = -1;
    int enableDebug = 0;
    int trycount = 0;
    int ret;

    memset(pcc, 0, sizeof(*pcc));

    /* Initialize the ENet */
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error (%s) occured while initializing ENet.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    /* Create the client host */
    pcc->client = enet_host_create(NULL, SHUTTLE_MAX_CLIENT_NUM, SHUTTLE_MAX_CHANNEL_NUM,
                                   SHUTTLE_MAX_INCOMING_BANDWIDTH, SHUTTLE_MAX_OUTGOING_BANDWIDTH);
    if (pcc->client == NULL) {
        fprintf(stderr, "An error (%s) occured while trying to create an ENet client host.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Connect to the server */
    enet_address_set_host(&address, SHUTTLE_SERVER_HOST);
    address.port = SHUTTLE_SERVER_PORT;
    pcc->peer = enet_host_connect(pcc->client, &address, SHUTTLE_MAX_CHANNEL_NUM, 0);
    if (pcc->peer == NULL) {
        fprintf(stderr, "No available peers for initializing an ENet connection.\n");
        exit(EXIT_FAILURE);
    }

    do {
        trycount++;
        printf("(Peer) Try to connect to server: the %dth tryouts.\n", trycount);
        if (enet_host_service(pcc->client, &event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            /* We can send packet to server only after we have received ENET_EVENT_TYPE_CONNECT */
            pcc->connected = 1;
            enet_address_get_host_ip(&event.peer->address, host_ip, sizeof(host_ip) - 1);
            snprintf(peer_info, sizeof(peer_info), "[%s:%d]", host_ip, event.peer->address.port);
            if (event.peer->data)
                free(event.peer->data);
            event.peer->data = malloc(strlen(peer_info) + 1);
            if (event.peer->data)
                strcpy(event.peer->data, peer_info);
            printf("(Peer) Connected to server (%s:%d).\n", host_ip, event.peer->address.port);
        }
    } while (trycount < 4 && !pcc->connected);

    if (!pcc->connected) {
        fprintf(stderr, "Fail to connect to server.\n");
        enet_peer_reset(pcc->peer);
        enet_host_destroy(pcc->client);
        exit(EXIT_FAILURE);
    }

    /* We do not block the main event dispatcher */
    ret = pthread_create(&pcc->thread, NULL, peer_chater, pcc);
    if (ret) {
        pcc->thread = 0;
        fprintf(stderr, "Fail to create thread.\n");
        goto cleanup_pos;
    }

    while (1) {
        /* Event dispatcher: MUST not be hanged up */
        int eventStatus = enet_host_service(pcc->client, &event, 10);
        if (eventStatus >= 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_NONE:
                /* Silence huge repeated NONE events */
                if (lastEvent != ENET_EVENT_TYPE_NONE) {
                    if (enableDebug)
                        printf("(Peer) No event.\n");
                }
                break;
            case ENET_EVENT_TYPE_CONNECT:
                /* Store any relevant client information here. */
                pcc->connected = 1;
                enet_address_get_host_ip(&event.peer->address, host_ip, sizeof(host_ip) - 1);
                snprintf(peer_info, sizeof(peer_info), "[%s:%d]", host_ip, event.peer->address.port);
                if (event.peer->data)
                    free(event.peer->data);
                event.peer->data = malloc(strlen(peer_info));
                if (event.peer->data)
                    strcpy(event.peer->data, peer_info);
                printf("(Peer) Connected to server (%s:%d).\n", host_ip, event.peer->address.port);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (event.channelID == SHUTTLE_CHANNEL_NOTIFY)
                    printf("(Peer) Got a notification message : %s.\n", (char*)event.packet->data);
                else
                    printf("(Peer) Got a chat message: %s.\n", (char*)event.packet->data);
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                /* A connected peer has either explicitly disconnected or timed out. */
                printf("(Peer) Connection status: %d.\n", pcc->connected);
                if (event.peer->data) {
                    printf("(Peer) %s is disconnected.\n", (char*)event.peer->data);
                    free(event.peer->data);
                } else {
                    /* We fail to receive CONNECT event becasue the server is down. */
                    enet_address_get_host_ip(&event.peer->address, host_ip, sizeof(host_ip) - 1);
                    snprintf(peer_info, sizeof(peer_info), "[%s:%d]", host_ip, event.peer->address.port);
                    printf("(Peer) Unknown (%s) connection is disconnected.\n", peer_info);
                }
                /* Reset the peer's information. */
                event.peer->data = NULL;
                pcc->connected = 0;
                lastEvent = -1;
                enet_peer_reset(event.peer);
                /* Reconnect the server */
                pcc->peer = enet_host_connect(pcc->client, &address, SHUTTLE_MAX_CHANNEL_NUM, 0);
                if (pcc->peer == NULL) {
                    fprintf(stderr, "No available peers for initializing an ENet connection.\n");
                    enet_host_destroy(pcc->client);
                    ret = EXIT_FAILURE;
                    goto cleanup_pos;
                }
                break;
            default:
                assert(0);
                break;
            }

            lastEvent = event.type;
        } else {
            fprintf(stderr, "(Peer) Something went wrong: %d.\n", eventStatus);
            lastEvent = -1;
            pcc->connected = 0;
            enet_peer_reset(pcc->peer);
            ret = eventStatus;
            goto cleanup_pos;
        }
    }

    ret = 0;

cleanup_pos:
    /* Terminate the chater thread. */
    pcc->terminated = 1;
    if (pcc->thread) {
        pthread_join(pcc->thread, NULL);
        pcc->thread = 0;
    }

    enet_host_destroy(pcc->client);
    printf("Client is terminated.\n");

    return ret;
}
