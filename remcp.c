#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSZ 128

static ssize_t robust_read(int fd, void *buf, size_t count)
{
    size_t total = 0;
    while (total < count)
    {
        ssize_t r = read(fd, (char *)buf + total, count - total);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            break;
        total += r;
    }
    return total;
}

static ssize_t robust_write(int fd, const void *buf, size_t count)
{
    size_t total = 0;
    while (total < count)
    {
        ssize_t w = write(fd, (const char *)buf + total, count - total);
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        total += w;
    }
    return total;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <ip_servidor> <arquivo>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    char *filename = argv[2];

    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Erro abrindo socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9000);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        perror("IP inválido");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erro na conexão");
        close(sockfd);
        exit(1);
    }

    // Envia o nome do arquivo
    size_t fname_len = strlen(filename);
    if (robust_write(sockfd, filename, fname_len) != (ssize_t)fname_len)
    {
        perror("Erro enviando nome do arquivo");
        close(sockfd);
        exit(1);
    }

    // Recebe offset do servidor
    off_t offset_servidor;
    ssize_t n = robust_read(sockfd, &offset_servidor, sizeof(offset_servidor));
    if (n != sizeof(offset_servidor))
    {
        perror("Erro lendo offset do servidor");
        close(sockfd);
        exit(1);
    }

    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        perror("Erro abrindo arquivo local");
        close(sockfd);
        exit(1);
    }

    if (fseeko(f, offset_servidor, SEEK_SET) < 0)
    {
        perror("Erro no fseeko");
        fclose(f);
        close(sockfd);
        exit(1);
    }

    char buffer[BUFSZ];
    size_t r;
    while ((r = fread(buffer, 1, BUFSZ, f)) > 0)
    {
        if (robust_write(sockfd, buffer, r) != (ssize_t)r)
        {
            perror("Erro enviando dados ao servidor");
            fclose(f);
            close(sockfd);
            exit(1);
        }
    }

    if (ferror(f))
    {
        perror("Erro lendo arquivo local");
    }

    fclose(f);
    close(sockfd);
    return 0;
}