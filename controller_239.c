#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 1024


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

    struct sockaddr_in server_addr;

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

    /* 8. Send LISTPROC command */
const char *listproc_message = "LISTPROC\n";

send(sock_fd,
     listproc_message,
     strlen(listproc_message),
     0);

    /* 9. Receive LISTPROC response */
{
    char listproc_buffer[1024];
    char listproc_data[16384];

    size_t total_received = 0;
    int chunk_received;

    listproc_data[0] = '\0';

    while (1)
    {
        memset(listproc_buffer, 0, sizeof(listproc_buffer));

        chunk_received = recv(sock_fd,
                              listproc_buffer,
                              sizeof(listproc_buffer) - 1,
                              0);

        if (chunk_received <= 0)
        {
            perror("recv");
            close(sock_fd);
            return 1;
        }

        listproc_buffer[chunk_received] = '\0';

        if (total_received + chunk_received
            < sizeof(listproc_data) - 1)
        {
            strcat(listproc_data, listproc_buffer);
            total_received += chunk_received;
        }

        if (strstr(listproc_data,
                   "LISTPROC END SID:9321\n") != NULL)
        {
            break;
        }
    }

    printf("Agent response:\n%s", listproc_data);
}

    
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


    /* 13. Send QUIT command */
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
