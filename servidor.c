#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORTA 5050
#define BACKLOG 10
#define BUFFER_SIZE 4096

/*determina o tipo baseado na extensão do arquivo*/
const char* get_content_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    return "application/octet-stream";
}
/*envia um arquivo ao cliente via socket*/
void send_file(int client, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        char *res = "HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Length: 13\r\n\r\n404 Not Found";
        send(client, res, strlen(res), 0);
        return;
    }

    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.0 200 OK\r\n"
             "Content-Length: %ld\r\n"
             "Content-Type: %s\r\n"
             "Connection: close\r\n\r\n",
             tamanho, get_content_type(path));
    send(client, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        if (send(client, buffer, n, 0) < 0) break;
    }
    fclose(f);
}
/*envia mensagem de erro 404*/
void send_404(int client) {
    char *res = "HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Length: 13\r\n\r\n404 Not Found";
    send(client, res, strlen(res), 0);
}
/*gera e envia listagem html do diretorio*/
void send_listagem(int client, const char *dirpath) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer),
             "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
             "Arquivos em %s:\n", dirpath);
    send(client, buffer, strlen(buffer), 0);

    DIR *d = opendir(dirpath);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
                send(client, buffer, strlen(buffer), 0);
            }
        }
        closedir(d);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <diretório>\n", argv[0]);
        return 1;
    }
    const char *dirpath = argv[1];
    //verifica se o diretorio existe e é acessível
    struct stat st;
    if (stat(dirpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Diretório inválido: %s\n", dirpath);
        return 1;
    }
    //cria o socket tcp
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }
    //configura o socket para reutilizar o endereço
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //configura a estrutura de endereço do servidor
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORTA);
    //associa o socket a uma porta com bind()
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro no bind");
        exit(1);
    }
    //listen() coloca o socket em modo de escuta
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Erro no listen");
        exit(1);
    }

    printf("Servidor rodando na porta %d, servindo: %s\n", PORTA, dirpath);
    printf("Acesse: http://localhost:%d\n", PORTA);
    //servidor aceita conexões
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("Erro no accept");
            continue;
        }

        char buffer[BUFFER_SIZE];
        int n = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            char metodo[16] = {0}, arquivo[256] = {0};
            
            sscanf(buffer, "%15s %255s", metodo, arquivo);

            if (strcmp(arquivo, "/") == 0) {
                arquivo[0] = '\0';
            }
            //servidor procura pelo diretorio o arquivo pedido pelo cliente
            //se existir, ele abre e envia ao cliente
            //se nao, exibe erro
            //se nao existe o index.html ele faz a listagem dos arquivos existentes
            if (strcmp(metodo, "GET") == 0) {
                char filepath[512];
                if (strlen(arquivo) == 0) {
                    snprintf(filepath, sizeof(filepath), "%s/index.html", dirpath);
                    if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                        printf("Servindo index.html\n");
                        send_file(client_fd, filepath);
                    } else {
                        printf("Servindo listagem (sem index.html)\n");
                        send_listagem(client_fd, dirpath);
                    }
                } else {
                    snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, arquivo);
                    if (stat(filepath, &st) == 0) {
                        if (S_ISREG(st.st_mode)) {
                            printf("Servindo arquivo: %s\n", filepath);
                            send_file(client_fd, filepath);
                        } else if (S_ISDIR(st.st_mode)) {
                            // diretório - tenta index.html ou lista
                            char index_path[1024];
                            snprintf(index_path, sizeof(index_path), "%s/index.html", filepath);
                            if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
                                send_file(client_fd, index_path);
                            } else {
                                send_listagem(client_fd, filepath);
                            }
                        } else {
                            send_404(client_fd);
                        }
                    } else {
                        printf("Arquivo não encontrado: %s\n", filepath);
                        send_404(client_fd);
                    }
                }
            } else {
                char *res = "HTTP/1.0 501 Not Implemented\r\nConnection: close\r\n\r\n";
                send(client_fd, res, strlen(res), 0);
            }
        }
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
