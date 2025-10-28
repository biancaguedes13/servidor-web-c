#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

void salvar_arquivo(const char *nome, const char *dados, size_t tamanho) {
    FILE *f = fopen(nome, "wb");
    if (!f) {
        perror("Erro ao criar arquivo");
        exit(1);
    }
    if (fwrite(dados, 1, tamanho, f) != tamanho) {
        perror("Erro ao escrever arquivo");
        fclose(f);
        exit(1);
    }
    fclose(f);
    printf("Arquivo salvo: %s (%zu bytes)\n", nome, tamanho);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s http://host[:porta]/caminho/arquivo\n", argv[0]);
        printf("Exemplos:\n");
        printf("  %s http://localhost:5050/index.html\n", argv[0]);
        printf("  %s http://localhost:5050/teste.pdf\n", argv[0]);
        printf("  %s http://localhost:5050/imagem.jpg\n", argv[0]);
        return 1;
    }

    char host[256] = {0};
    char path[256] = "";
    int porta = 80;

    char *url = argv[1];
    if (strncmp(url, "http://", 7) != 0) {
        fprintf(stderr, "Erro: URL deve começar com http://\n");
        return 1;
    }
    url += 7;

    // separa o url recebido em host, porta e caminho
    char *path_start = strchr(url, '/');
    if (path_start) {
        strncpy(path, path_start + 1, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        *path_start = '\0';
    }

    
    char *colon = strchr(url, ':');
    if (colon) {
        *colon = '\0';
        porta = atoi(colon + 1);
        if (porta <= 0 || porta > 65535) {
            fprintf(stderr, "Erro: Porta inválida: %d\n", porta);
            return 1;
        }
    }
    
    strncpy(host, url, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';

    printf("Conectando a: %s:%d\n", host, porta);
    printf("Baixando arquivo: /%s\n", path);

    struct hostent *server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "Erro: Não foi possível resolver o host: %s\n", host);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    // configura endereço do servidor
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(porta);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // conecta ao servidor
    printf("Conectando ao servidor...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        perror("Erro: Não foi possível conectar ao servidor");
        close(sock);
        return 1;
    }
    printf("Conectado!\n");

    // enviar requisição
    char req[512];
    if (strlen(path) > 0) {
        snprintf(req, sizeof(req), "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
    } else {
        snprintf(req, sizeof(req), "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
    }
    
    printf("Enviando requisição...\n");
    if (send(sock, req, strlen(req), 0) < 0) {
        perror("Erro ao enviar requisição");
        close(sock);
        return 1;
    }

    // recebe resposta
    FILE *temp_file = tmpfile();
    if (!temp_file) {
        perror("Erro ao criar arquivo temporário");
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    int n;
    size_t total_recebido = 0;

    printf("Recebendo resposta...\n");
    while ((n = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buffer, 1, n, temp_file) != (size_t)n) {
            perror("Erro ao escrever arquivo temporário");
            fclose(temp_file);
            close(sock);
            return 1;
        }
        total_recebido += n;
    }
    close(sock);

    if (n < 0) {
        perror("Erro ao receber dados");
        fclose(temp_file);
        return 1;
    }

    if (total_recebido == 0) {
        fprintf(stderr, "Erro: Nenhum dado recebido do servidor\n");
        fclose(temp_file);
        return 1;
    }

    // processar resposta
    rewind(temp_file);
    char *resposta = malloc(total_recebido + 1);
    if (!resposta) {
        perror("Erro: Falha na alocação de memória");
        fclose(temp_file);
        return 1;
    }
    
    size_t bytes_lidos = fread(resposta, 1, total_recebido, temp_file);
    if (bytes_lidos != total_recebido) {
        perror("Erro ao ler arquivo temporário");
        free(resposta);
        fclose(temp_file);
        return 1;
    }
    resposta[total_recebido] = '\0';
    fclose(temp_file);

    //códigos de erro HTTP
    if (strstr(resposta, "404 Not Found")) {
        fprintf(stderr, "Erro: Arquivo não encontrado no servidor (404)\n");
        free(resposta);
        return 1;
    }
    
    if (strstr(resposta, "500")) {
        fprintf(stderr, "Erro: Erro interno do servidor (500)\n");
        free(resposta);
        return 1;
    }

    // listagem HTML (quando deveria ser um arquivo)
    if (strstr(resposta, "Listagem de Arquivos") && strlen(path) > 0) {
        fprintf(stderr, "Erro: Arquivo não encontrado - servidor retornou listagem\n");
        free(resposta);
        return 1;
    }

    // encontra início do corpo (fim dos cabeçalhos)
    char *corpo = strstr(resposta, "\r\n\r\n");
    if (!corpo) {
        fprintf(stderr, "Erro: Resposta HTTP inválida\n");
        free(resposta);
        return 1;
    }
    corpo += 4;

    // Determinar nome do arquivo para salvar
    char nome_arquivo[256];
    if (strlen(path) > 0) {
        char *nome = strrchr(path, '/');
        nome = nome ? nome + 1 : path;
        if (strlen(nome) > 0) {
            // Usar snprintf para evitar warnings
            snprintf(nome_arquivo, sizeof(nome_arquivo), "%s", nome);
        } else {
            strcpy(nome_arquivo, "index.html");
        }
    } else {
        strcpy(nome_arquivo, "index.html");
    }

    printf("Salvando arquivo: %s\n", nome_arquivo);
    salvar_arquivo(nome_arquivo, corpo, total_recebido - (corpo - resposta));
    
    free(resposta);
    printf("Download concluído com sucesso!\n");

    return 0;
}