/*
 * Global configuration for Shuttle
 * Created initially by Jack Waller on 2016-02-18
 *
 * https://github.com/nxtreaming
 *
 */
#ifndef SHUTTLE_CONFIG_H
#define SHUTTLE_CONFIG_H 1

#define SHUTTLE_MAX_CLIENT_NUM              128
#define SHUTTLE_MAX_CHANNEL_NUM             5
#define SHUTTLE_MAX_INCOMING_BANDWIDTH      0
#define SHUTTLE_MAX_OUTGOING_BANDWIDTH      0

#define SHUTTLE_CHANNEL_NOTIFY              0

/* MUST change to your server host or "localhost". */
#define SHUTTLE_SERVER_HOST                 "localhost"
#define SHUTTLE_SERVER_PORT                 20123

#endif /* SHUTTLE_CONFIG_H */
