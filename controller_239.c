#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 1024
#define UDP_PORT 9411

int recv_line(int sock_fd, char *buffer, int size)
{
    int total = 0;
    char ch;

    while (total < size - 1)
    {
        int n = recv(sock_fd, &ch, 1, 0);

        if (n <= 0)
            return n;

        buffer[total++] = ch;

        if (ch == '\n')
            break;
    }

    buffer[total] = '\0';

    return total;
}

int main(void)
{
    int sock_fd;
int udp_fd;

struct sockaddr_in server_addr;
struct sockaddr_in udp_addr;

char buffer[BUFFER_SIZE];

    /* 1. Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* 2. Configure Agent address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    /* 3. Connect to Agent */
    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to RemoteOps Agent.\n");

        /* 4. Send AUTH command */
    const char *auth_message = "AUTH OPS-1239\n";

    send(sock_fd,
         auth_message,
         strlen(auth_message),
         0);

    /* 5. Receive AUTH response */
    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(sock_fd,
                              buffer,
                              sizeof(buffer) - 1,
                              0);

    if (bytes_received <= 0)
    {
        perror("recv");
        close(sock_fd);
        return 1;
    }

    buffer[bytes_received] = '\0';

    printf("Agent response: %s", buffer);

    /* 6. Send SYSINFO command */
    const char *sysinfo_message = "SYSINFO\n";

    send(sock_fd,
         sysinfo_message,
         strlen(sysinfo_message),
         0);

    /* 7. Receive SYSINFO response */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv(sock_fd,
                          buffer,
                          sizeof(buffer) - 1,
                          0);

    if (bytes_received <= 0)
    {
        perror("recv");
        close(sock_fd);
        return 1;
    }

    buffer[bytes_received] = '\0';

    printf("Agent response: %s", buffer);

 
           /* 10. Send PUT command */
    {
        FILE *file;
        char file_buffer[1024];
        long filesize;
        size_t bytes_read;

        file = fopen("testfile.txt", "rb");

        if (file == NULL)
        {
            perror("fopen");
            close(sock_fd);
            return 1;
        }

        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        rewind(file);

        {
            char put_command[256];

            snprintf(put_command,
                     sizeof(put_command),
                     "PUT testfile.txt %ld\n",
                     filesize);

            send(sock_fd,
                 put_command,
                 strlen(put_command),
                 0);
sleep(1);
        }

        while ((bytes_read = fread(file_buffer,
                                   1,
                                   sizeof(file_buffer),
                                   file)) > 0)
        {
            send(sock_fd,
                 file_buffer,
                 bytes_read,
                 0);
        }

        fclose(file);

        /* Receive PUT response */
        memset(buffer, 0, sizeof(buffer));

        bytes_received = recv(sock_fd,
                              buffer,
                              sizeof(buffer) - 1,
                              0);

        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';

            printf("Agent response: %s", buffer);
        }
    }

      
        /* 11. Send GET command */
    send(sock_fd,
         "GET testfile.txt\n",
         strlen("GET testfile.txt\n"),
         0);

        /* Receive GET response header */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv_line(sock_fd,
                               buffer,
                               sizeof(buffer));

    if (bytes_received > 0)
    {
        long filesize;
        long total_received = 0;
        int received;
        FILE *download_file;
        char file_buffer[1024];

        buffer[bytes_received] = '\0';

        printf("Agent response: %s", buffer);

        if (sscanf(buffer,
                   "GET OK BYTES:%ld",
                   &filesize) == 1)
        {
            download_file = fopen("downloaded_testfile.txt", "wb");

            if (download_file == NULL)
            {
                perror("fopen");
                close(sock_fd);
                return 1;
            }

            while (total_received < filesize)
            {
                long remaining = filesize - total_received;

                int receive_size =
                    remaining < (long)sizeof(file_buffer)
                    ? (int)remaining
                    : (int)sizeof(file_buffer);

                received = recv(sock_fd,
                                file_buffer,
                                receive_size,
                                0);

                if (received <= 0)
                    break;

                fwrite(file_buffer,
                       1,
                       received,
                       download_file);

                total_received += received;
            }

            fclose(download_file);

            printf("Downloaded %ld bytes.\n",
                   total_received);
        }
    }


    /* 12. Send EXEC command */

send(sock_fd,
     "EXEC DATE\n",
     strlen("EXEC DATE\n"),
     0);

    /* Receive EXEC response */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv(sock_fd,
                          buffer,
                          sizeof(buffer) - 1,
                          0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Agent response: %s", buffer);
    }

         /* 13. Start UDP monitoring */

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd < 0)
    {
        perror("UDP socket");
    }
    else
    {
        memset(&udp_addr, 0, sizeof(udp_addr));

        udp_addr.sin_family = AF_INET;
        udp_addr.sin_port = htons(UDP_PORT);
        udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(udp_fd,
                 (struct sockaddr *)&udp_addr,
                 sizeof(udp_addr)) < 0)
        {
            perror("UDP bind");
            close(udp_fd);
            udp_fd = -1;
        }
    }

    /* Send MONITOR START */
    send(sock_fd,
         "MONITOR START\n",
         strlen("MONITOR START\n"),
         0);

    /* Receive MONITOR START response */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv_line(sock_fd,
                               buffer,
                               sizeof(buffer));

    if (bytes_received > 0)
    {
        printf("Agent response: %s", buffer);
    }

    /* Receive three UDP monitoring packets */
    if (udp_fd >= 0)
    {
        struct sockaddr_in monitor_addr;
        socklen_t monitor_addr_len =
            sizeof(monitor_addr);

        int i;

        for (i = 0; i < 3; i++)
        {
            memset(buffer, 0, sizeof(buffer));

            bytes_received =
                recvfrom(udp_fd,
                         buffer,
                         sizeof(buffer) - 1,
                         0,
                         (struct sockaddr *)&monitor_addr,
                         &monitor_addr_len);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                printf("UDP monitor: %s", buffer);
            }
        }
    }

    /* Send MONITOR STOP */
    send(sock_fd,
         "MONITOR STOP\n",
         strlen("MONITOR STOP\n"),
         0);

    /* Receive MONITOR STOP response */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv_line(sock_fd,
                               buffer,
                               sizeof(buffer));

    if (bytes_received > 0)
    {
        printf("Agent response: %s", buffer);
    }

    if (udp_fd >= 0)
    {
        close(udp_fd);
    }

    /* 14. Send QUIT command */
    send(sock_fd,
         "QUIT\n",
         strlen("QUIT\n"),
         0);

    /* Receive QUIT response */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv(sock_fd,
                          buffer,
                          sizeof(buffer) - 1,
                          0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Agent response: %s", buffer);
    }


    /* 14. Close connection */
    close(sock_fd);

    return 0;

    /* 12. Close connection */
    close(sock_fd);

    return 0;
}
