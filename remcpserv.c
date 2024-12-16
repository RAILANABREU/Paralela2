#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORTA 9000
#define BUFSZ 128 // tamanho do chunk para .part

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

int main()
{
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Erro abrindo socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORTA);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erro no bind");
        exit(1);
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("Erro no listen");
        exit(1);
    }

    printf("Servidor aguardando conexões na porta %d...\n", PORTA);

    for (;;)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("Erro no accept");
            continue;
        }

        printf("Conexão aceita de %s:%d\n",
               inet_ntoa(cli_addr.sin_addr),
               ntohs(cli_addr.sin_port));

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Erro no fork");
            close(newsockfd);
            continue;
        }

        if (pid == 0)
        {
            // Processo filho
            close(sockfd);

            // Lê o nome do arquivo enviado pelo cliente
            char filename[1024];
            ssize_t n = read(newsockfd, filename, sizeof(filename) - 1);
            if (n <= 0)
            {
                perror("Erro lendo nome do arquivo");
                close(newsockfd);
                exit(1);
            }
            filename[n] = '\0';

            // Obtém o diretório HOME
            const char *home_dir = getenv("HOME");
            if (home_dir == NULL)
            {
                home_dir = "/tmp";
            }

            char final_path[2048];
            snprintf(final_path, sizeof(final_path), "%s/Downloads/%s", home_dir, filename);

            char partname[1100];
            snprintf(partname, sizeof(partname), "%s.part", filename);

            FILE *f = fopen(partname, "ab+");
            if (!f)
            {
                perror("Erro abrindo .part");
                close(newsockfd);
                exit(1);
            }

            fseeko(f, 0, SEEK_END);
            off_t offset = ftello(f);

            // Envia o offset ao cliente
            if (robust_write(newsockfd, &offset, sizeof(offset)) != sizeof(offset))
            {
                perror("Erro ao enviar offset");
                fclose(f);
                close(newsockfd);
                exit(1);
            }

            printf("Recebendo arquivo: %s\n", filename);

            // Recebe o arquivo do cliente
            char buffer[BUFSZ];
            for (;;)
            {
                ssize_t r = read(newsockfd, buffer, BUFSZ);
                if (r < 0)
                {
                    perror("Erro lendo do socket");
                    fclose(f);
                    close(newsockfd);
                    exit(1);
                }
                else if (r == 0)
                {
                    // Fim da transmissão
                    break;
                }
                else
                {
                    size_t written = fwrite(buffer, 1, r, f);
                    if (written < (size_t)r)
                    {
                        perror("Erro escrevendo no arquivo .part");
                        fclose(f);
                        close(newsockfd);
                        exit(1);
                    }
                }
            }

            fflush(f);
            fsync(fileno(f)); // opcional: garante flush no disco
            fclose(f);

            // Renomeia o arquivo parcial
            if (rename(partname, final_path) < 0)
            {
                perror("Erro ao renomear arquivo final");
            }
            else
            {
                printf("Arquivo salvo com sucesso em: %s\n", final_path);
            }

            close(newsockfd);
            exit(0);
        }
        else
        {
            // Processo pai
            close(newsockfd);
        }
    }

    close(sockfd);
    return 0;
}