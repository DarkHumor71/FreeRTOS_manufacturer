#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include "status_server.h"
#include "petri_net.h"
#include <stdio.h>
#include <string.h>

static int build_status_payload(char* buffer, size_t size) {
    if (size == 0) {
        return 0;
    }

    int offset = snprintf(buffer, size, "{\"places\":[");
    for (int i = 0; i < manufacturing_net.num_places && offset < (int)size; i++) {
        int tokens = get_place_tokens(i);
        int written = snprintf(buffer + offset, size - offset,
            "{\"name\":\"%s\",\"tokens\":%d}%s",
            manufacturing_net.places[i].name,
            tokens,
            (i + 1 < manufacturing_net.num_places) ? "," : "");
        if (written < 0) {
            break;
        }
        offset += written;
    }

    if (offset < (int)size) {
        offset += snprintf(buffer + offset, size - offset, "]}");
    }

    // After building payload, clear dirty flag
    extern atomic_bool status_dirty;
    atomic_store(&status_dirty, false);
    return offset >= (int)size ? (int)size - 1 : offset;
}

void task_status_server(void* params) {
    (void)params;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        vTaskDelete(NULL);
        return;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        WSACleanup();
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in service;
    ZeroMemory(&service, sizeof(service));
    service.sin_family = AF_INET;
    // Bind to all network interfaces for external access
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(STATUS_SERVER_PORT);

    if (bind(listen_socket, (struct sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
        closesocket(listen_socket);
        WSACleanup();
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_socket, STATUS_SERVER_BACKLOG) == SOCKET_ERROR) {
        closesocket(listen_socket);
        WSACleanup();
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        SOCKET client = accept(listen_socket, NULL, NULL);
        if (client == INVALID_SOCKET) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Always respond immediately with the latest state
        char request[128];
        recv(client, request, sizeof(request) - 1, 0);

        char payload[STATUS_JSON_BUFFER];
        int payload_len = build_status_payload(payload, sizeof(payload));
        if (payload_len < 0) {
            payload_len = 0;
            payload[0] = '\0';
        }

        char response[STATUS_RESPONSE_BUFFER];
        int response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n" // CORS header
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            payload_len,
            payload);

        send(client, response, response_len, 0);
        shutdown(client, SD_BOTH);
        closesocket(client);
    }

    closesocket(listen_socket);
    WSACleanup();
    vTaskDelete(NULL);
}