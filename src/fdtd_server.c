#define _POSIX_C_SOURCE 200809L

#include "../includes/net_util.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>


#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 64

#ifndef MAX_ACTIVE_THREADS
#define MAX_ACTIVE_THREADS 32 //K handlers active 
#endif

static volatile sig_atomic_t g_running = 1;
static int listen_file_descriptor = -1;

static sem_t g_counting_sem; //counting semaphore | K permissions, used by handlers

typedef struct {
    int file_descriptor;
    unsigned long connection_id;
} connection_t;

static void on_sigint(int signum)
{
    (void)signum;
    g_running = 0;
}

static int install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;

    //sigaction(int signum, const struct sigaction *act struct sigaction *oldact)
    //SIGINT = the signal that Ctrl-C sends.
    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}


static void sem_wait_retry(sem_t *s){
    //while thread is interrupted, with sem_wait, if it doesnt have the semaphore to continue, retry and wait until it gets the semaphore.
    while (sem_wait(s) < 0 && errno == EINTR){}
    //sem_wait() sub 1 to the value of the semaphore, if value is 0, thread sleeps.
}

static void *handle_connection(void *arg)
{
    connection_t *conn = arg; //thread
    char line[LINE_MAX_LEN];

    ssize_t n = nu_read_line(conn->file_descriptor, line, sizeof line);
    if (n > 0)
        //This is when the command is accepted, it should answer back. IMPLEMENT THIS IN THE FUTURE
        (void)nu_send_line(conn->file_descriptor, "ERROR not implemented");

    else if (n == 0)
        (void)nu_send_line(conn->file_descriptor, "ERROR empty line");

    else if (errno == EMSGSIZE)
        (void)nu_send_line(conn->file_descriptor, "ERROR line too long");
    

    if (close(conn->file_descriptor) < 0)
        perror("close(file_descriptor)");

    free(conn);
    sem_post(&g_counting_sem);
    return NULL;

}

static unsigned short parse_port(int argc, char **argv)
{
    if (argc < 2)
    {
        return DEFAULT_PORT;
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0' ||
        value <= 0 || value > 65535) {
        fprintf(stderr, "invalid port '%s', using %d\n", argv[1], DEFAULT_PORT);
        return DEFAULT_PORT;
        }

    return (unsigned short)value;
}

int main(int argc, char **argv)
{
    if (install_signal_handlers() < 0)
    {
        return EXIT_FAILURE;
    }

    if (sem_init(&g_counting_sem, 0, MAX_ACTIVE_THREADS) < 0){
        perror("sem_init");
        return EXIT_FAILURE;
    }

    unsigned short port = parse_port(argc, argv);

    listen_file_descriptor = nu_listen(port, LISTEN_BACKLOG);

    if (listen_file_descriptor < 0)
    {
        sem_destroy(&g_counting_sem);
        return EXIT_FAILURE;
    }

    printf("listening on port %u — Ctrl-C to stop\n", port);
    fflush(stdout);

    unsigned long accepted = 0; 

    while (g_running)
    {                        
         
        if (sem_wait(&g_counting_sem) < 0){
            
            if (errno == EINTR){
                continue;             
            }

            perror("sem_wait");
            break;
        }
        
        
                                    //accept(int socket, struct sockaddr *address, socklen_t *address_len)
        int client_file_descriptor = accept(listen_file_descriptor, NULL, NULL);
        if (client_file_descriptor < 0)
        {
            sem_post(&g_counting_sem);
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept");
            break;
        }
        
        connection_t *conn = malloc(sizeof(connection_t));

        if (conn == NULL)
        {
            fprintf(stderr, "out of memory, dropping connection\n");
            close(client_file_descriptor);
            sem_post(&g_counting_sem); 
            continue;
        }

        conn->file_descriptor = client_file_descriptor;
        conn->connection_id = ++accepted;

        pthread_t thread_id;

        int pthread_created = pthread_create(&thread_id, NULL, handle_connection, conn);

        if (pthread_created != 0)
        {
            fprintf(stderr, "pthread_create failed %s\n", strerror(pthread_created));
            close(client_file_descriptor);
            free(conn);
            --accepted;
            sem_post(&g_counting_sem); 
            continue;
        }
        
        pthread_created = pthread_detach(thread_id);
        if (pthread_created != 0)
        {
            fprintf(stderr, "pthread_detach failed %s\n", strerror(pthread_created));
        }

    }

    if (close(listen_file_descriptor))
    {
        perror("close(listen_file_descriptor)");
    }

    for (int i = 0; i < MAX_ACTIVE_THREADS; i++){
        sem_wait_retry(&g_counting_sem);
    }

    sem_destroy(&g_counting_sem);

    printf("server stopped (%lu connections accepted)\n", accepted);

    return EXIT_SUCCESS;
}