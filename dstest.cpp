#include "errors.h"

/* Test for Creatable parsing *
#include "factory.h"

static uint8_t msgbuf[] = {
0xB4, 0x03, 0x01, 0x10, 0x00, 0x00, 0xCA, 0x74, 0x71, 0x4A, 0x70, 0x8E,
0x01, 0x00, 0x99, 0x58, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x06, 0xFF,
0x04, 0x00, 0x01, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x04, 0xF0, 0xB2, 0x9E,
0x93, 0x9A, 0x02, 0x00, 0x00, 0x00, 0x99, 0x58, 0x00, 0x00,
};

int main(int argc, char* argv[])
{
    DS::BufferStream stream(msgbuf, sizeof(msgbuf));
    uint16_t ctype = stream.read<uint16_t>();
    MOUL::Creatable* cre = MOUL::Factory::Create(ctype);
    cre->read(&stream);
    DS_DASSERT(stream.atEof());
    cre->unref();

    return 0;
}
//*/

/* Test for Network I/O */
#include "NetIO/MsgChannel.h"
#include "NetIO/SockIO.h"
#include <list>
#include <cstdio>
#include <unistd.h>

std::list<DS::SocketHandle> s_clients;
pthread_mutex_t s_clientMutex;

DS::MsgChannel s_bcastChannel;
void* dm_broadcast(void*)
{
    bool stop = false;
    while (!stop) {
        DS::FifoMessage* msg = s_bcastChannel.getMessage();
        switch (msg->m_messageType) {
        case 0:
            /* Close all clients and shut down */
            pthread_mutex_lock(&s_clientMutex);
            for (std::list<DS::SocketHandle>::iterator sock_iter = s_clients.begin();
                 sock_iter != s_clients.end(); ++sock_iter)
                DS::CloseSock(*sock_iter);
            pthread_mutex_unlock(&s_clientMutex);
            stop = true;
            break;
        case 1:
            /* Send message */
            pthread_mutex_lock(&s_clientMutex);
            for (std::list<DS::SocketHandle>::iterator sock_iter = s_clients.begin();
                 sock_iter != s_clients.end(); ++sock_iter) {
                const char* text = reinterpret_cast<const char*>(msg->m_payload);
                DS::SendBuffer(*sock_iter, reinterpret_cast<const uint8_t*>(text), strlen(text));
            }
            pthread_mutex_unlock(&s_clientMutex);
            break;
        case 2:
            /* Recv message */
            {
                const char* text = reinterpret_cast<const char*>(msg->m_payload);
                fprintf(stdout, "%s", text);
                fflush(stdout);
                delete[] text;
            }
            break;
        }
        delete msg;
    }

    for (int i=0; i<50; ++i) {
        pthread_mutex_lock(&s_clientMutex);
        size_t alive = s_clients.size();
        pthread_mutex_unlock(&s_clientMutex);
        if (alive == 0)
            return 0;
        usleep(100000);
    }
    fprintf(stderr, "Clients didn't die after 5 seconds!");
    return 0;
}

void* wk_sclient(void* sockp)
{
    DS::SocketHandle sock = reinterpret_cast<DS::SocketHandle>(sockp);

    for ( ;; ) {
        DS::Blob* blob = DS::RecvBuffer(sock);
        if (blob == 0)
            break;
        char* text = new char[blob->size() + 1];
        memcpy(text, blob->buffer(), blob->size());
        text[blob->size()] = 0;
        s_bcastChannel.putMessage(2, text);
        blob->unref();
    }
    pthread_mutex_lock(&s_clientMutex);
    std::list<DS::SocketHandle>::iterator sock_iter = s_clients.begin();
    while (sock_iter != s_clients.end()) {
        if (*sock_iter == sock)
            sock_iter = s_clients.erase(sock_iter);
        else
            ++sock_iter;
    }
    DS::FreeSock(sock);
    pthread_mutex_unlock(&s_clientMutex);
    return 0;
}

void* dm_lobby(void* sockp)
{
    DS::SocketHandle listenSock = reinterpret_cast<DS::SocketHandle>(sockp);
    try {
        for ( ;; ) {
            DS::SocketHandle client = DS::AcceptSock(listenSock);
            if (!client)
                break;

            pthread_mutex_lock(&s_clientMutex);
            s_clients.push_back(client);
            pthread_mutex_unlock(&s_clientMutex);

            pthread_t worker;
            pthread_create(&worker, 0, &wk_sclient, reinterpret_cast<void*>(client));
            pthread_detach(worker);
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "Assertion failed at %s:%ld\n    %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    }
    DS::FreeSock(listenSock);
    return 0;
}

int main(int argc, char* argv[])
{
    DS::SocketHandle listenSock;
    try {
        listenSock = DS::BindSocket(0, "14505");
        DS::ListenSock(listenSock);
    } catch (DS::AssertException ex) {
        fprintf(stderr, "Assertion failed at %s:%ld\n    %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        return EXIT_FAILURE;
    }

    /* Ladies and Gentlemen, start your threads! */
    pthread_mutex_init(&s_clientMutex, 0);
    pthread_t listenThread, daemonThread;
    pthread_create(&daemonThread, 0, &dm_broadcast, 0);
    pthread_create(&listenThread, 0, &dm_lobby, reinterpret_cast<void*>(listenSock));

    char cmdbuf[256];
    while (fgets(cmdbuf, 256, stdin))
        s_bcastChannel.putMessage(1, cmdbuf);

    s_bcastChannel.putMessage(0);
    fprintf(stderr, "Waiting to terminate\n");
    DS::CloseSock(listenSock);
    pthread_join(listenThread, 0);
    pthread_join(daemonThread, 0);
    pthread_mutex_destroy(&s_clientMutex);
    return 0;
}
//*/
