#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#define PORT 9410
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);


    char buffer[BUFFER_SIZE];

    /* 1. Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* 2. Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* 3. Bind socket to port 9410 */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* 4. Start listening */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent listening on TCP port %d...\n", PORT);

        /* 5. Accept multiple Controller connections */
    signal(SIGCHLD, SIG_IGN);

    while (1)
    {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Controller connected.\n");

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid > 0)
        {
            /* Parent continues accepting new Controllers */
            close(client_fd);
            continue;
        }

        /* Child handles this Controller */
        close(server_fd);

        break;
    }


        /* 6. Receive commands */
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_received = recv(client_fd,
                                  buffer,
                                  sizeof(buffer) - 1,
                                  0);

        if (bytes_received < 0)
        {
            perror("recv");
            break;
        }

        if (bytes_received == 0)
        {
            printf("Controller disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        printf("Received: %s", buffer);

        /* 7. Check AUTH command */
        if (strcmp(buffer, "AUTH OPS-1239\n") == 0)
        {
            const char *response = "AUTH OK SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);
        }
        /* 8. Check SYSINFO command */
                else if (strcmp(buffer, "SYSINFO\n") == 0)
       {
        FILE *cpu_file;
        FILE *mem_file;
        FILE *uptime_file;

        char cpu_info[256];
        char mem_line[256];

        unsigned long mem_total = 0;
        unsigned long mem_available = 0;

        double uptime_seconds = 0.0;

        char response[1024];

        /* Read CPU model */
        cpu_file = fopen("/proc/cpuinfo", "r");

        if (cpu_file == NULL)
        {
            const char *error_response =
                "SYSINFO ERROR SID:9321\n";

            send(client_fd,
                 error_response,
                 strlen(error_response),
                 0);

            continue;
        }

        cpu_info[0] = '\0';

        while (fgets(cpu_info,
                     sizeof(cpu_info),
                     cpu_file) != NULL)
        {
            if (strncmp(cpu_info, "model name", 10) == 0)
            {
                cpu_info[strcspn(cpu_info, "\n")] = '\0';
                break;
            }
        }

        fclose(cpu_file);

        /* Read memory information */
        mem_file = fopen("/proc/meminfo", "r");

        if (mem_file != NULL)
        {
            while (fgets(mem_line,
                         sizeof(mem_line),
                         mem_file) != NULL)
            {
                if (sscanf(mem_line,
                           "MemTotal: %lu kB",
                           &mem_total) == 1)
                {
                    continue;
                }

                if (sscanf(mem_line,
                           "MemAvailable: %lu kB",
                           &mem_available) == 1)
                {
                    continue;
                }
            }

            fclose(mem_file);
        }

        /* Read system uptime */
        uptime_file = fopen("/proc/uptime", "r");

        if (uptime_file != NULL)
        {
            fscanf(uptime_file,
                   "%lf",
                   &uptime_seconds);

            fclose(uptime_file);
        }

        /* Build SYSINFO response */
        snprintf(response,
                 sizeof(response),
                 "SYSINFO OK %s MEM_TOTAL_KB:%lu MEM_AVAILABLE_KB:%lu UPTIME_SECONDS:%.0f SID:9321\n",
                 cpu_info,
                 mem_total,
                 mem_available,
                 uptime_seconds);

        send(client_fd,
             response,
             strlen(response),
             0);
    }
     
   
            /* 9. Check LISTPROC command */
    else if (strcmp(buffer, "LISTPROC\n") == 0)
    {
        FILE *process_file;
        char process_line[256];

        process_file = popen("ps -eo pid,comm", "r");

        if (process_file == NULL)
        {
            const char *error_response =
                "LISTPROC ERROR SID:9321\n";

            send(client_fd,
                 error_response,
                 strlen(error_response),
                 0);

            continue;
        }

        /* Send LISTPROC header */
        {
            const char *header =
                "LISTPROC OK SID:9321\n";

            send(client_fd,
                 header,
                 strlen(header),
                 0);
        }

        /* Send each process as a separate line */
        while (fgets(process_line,
                     sizeof(process_line),
                     process_file) != NULL)
        {
            char process_response[512];

            snprintf(process_response,
                     sizeof(process_response),
                     "%s SID:9321\n",
                     process_line);

            send(client_fd,
                 process_response,
                 strlen(process_response),
                 0);
        }

        pclose(process_file);

        /* Send end marker */
        {
            const char *end_response =
                "LISTPROC END SID:9321\n";

            send(client_fd,
                 end_response,
                 strlen(end_response),
                 0);
        }
    }

                /* 10. Check PUT command */
    else if (strncmp(buffer, "PUT ", 4) == 0)
    {
        char filename[256];
        long filesize;
        FILE *file;
        char filepath[512];
        char file_buffer[1024];
        long total_received = 0;
        int received;

        if (sscanf(buffer + 4, "%255s %ld",
                   filename, &filesize) != 2)
        {
            const char *response =
                "PUT ERROR SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            continue;
        }

        snprintf(filepath,
                 sizeof(filepath),
                 "./agentfiles/IT24101239/%s",
                 filename);

        file = fopen(filepath, "wb");

        if (file == NULL)
        {
            const char *response =
                "PUT ERROR SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            continue;
        }

        while (total_received < filesize)
        {
            long remaining = filesize - total_received;

            int receive_size =
                remaining < (long)sizeof(file_buffer)
                ? (int)remaining
                : (int)sizeof(file_buffer);

            received = recv(client_fd,
                            file_buffer,
                            receive_size,
                            0);

            if (received <= 0)
                break;

            fwrite(file_buffer,
                   1,
                   received,
                   file);

            total_received += received;
        }

        fclose(file);

        if (total_received == filesize)
        {
            char response[128];

            snprintf(response,
                     sizeof(response),
                     "PUT OK BYTES:%ld SID:9321\n",
                     total_received);

            send(client_fd,
                 response,
                 strlen(response),
                 0);
        }
        else
        {
            const char *response =
                "PUT ERROR SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);
        }
    }


        /* 11. Check GET command */
    else if (strncmp(buffer, "GET ", 4) == 0)
    {
        char filename[256];
        char filepath[512];
        FILE *file;
        char file_buffer[1024];
        long filesize;
        size_t bytes_read;

        if (sscanf(buffer + 4, "%255s", filename) != 1)
        {
            const char *response =
                "GET ERROR SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            continue;
        }

        snprintf(filepath,
                 sizeof(filepath),
                 "./agentfiles/IT24101239/%s",
                 filename);

        file = fopen(filepath, "rb");

        if (file == NULL)
        {
            const char *response =
                "GET ERROR SID:9321\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            continue;
        }

        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        rewind(file);

        {
            char response[128];

            snprintf(response,
                     sizeof(response),
                     "GET OK BYTES:%ld SID:9321\n",
                     filesize);

            send(client_fd,
                 response,
                 strlen(response),
                 0);
        }

        while ((bytes_read =
                fread(file_buffer,
                      1,
                      sizeof(file_buffer),
                      file)) > 0)
        {
            send(client_fd,
                 file_buffer,
                 bytes_read,
                 0);
        }

        fclose(file);
    }


   
     /* 12. Check QUIT command */
    else if (strcmp(buffer, "QUIT\n") == 0)
    {
        const char *response =
            "BYE SID:9321\n";

        send(client_fd,
             response,
             strlen(response),
             0);

        break;
    }
}


/* Close connection */
close(client_fd);
close(server_fd);

return 0;
}
