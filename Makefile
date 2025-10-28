CC = gcc
CFLAGS = -Wall -Wextra -O2

CLIENTE_SRC = cliente.c
SERVIDOR_SRC = servidor.c
CLIENTE_BIN = cliente
SERVIDOR_BIN = servidor
TEST_DIR = site_teste
PORTA = 5050

.PHONY: all server client clean help

# Alvo padrão - compila tudo
all: $(CLIENTE_BIN) $(SERVIDOR_BIN)
	@echo "Compilacao concluida!"
	@echo "Use 'make server' para iniciar o servidor"

# Compilar cliente
$(CLIENTE_BIN): $(CLIENTE_SRC)
	@echo "Compilando cliente..."
	@$(CC) $(CFLAGS) $(CLIENTE_SRC) -o $(CLIENTE_BIN)

# Compilar servidor
$(SERVIDOR_BIN): $(SERVIDOR_SRC)
	@echo "Compilando servidor..."
	@$(CC) $(CFLAGS) $(SERVIDOR_SRC) -o $(SERVIDOR_BIN)

# Iniciar servidor (usa a pasta site_teste que ja existe)
server: $(SERVIDOR_BIN)
	@echo "Iniciando servidor..."
	@echo "Servindo arquivos de: $(TEST_DIR)"
	@echo "Acesse: http://localhost:$(PORTA)"
	@./$(SERVIDOR_BIN) $(TEST_DIR)

# Testar cliente (baixa index.html automaticamente)
client: $(CLIENTE_BIN)
	@echo "Testando cliente..."
	@./$(CLIENTE_BIN) http://localhost:$(PORTA)/

# Listar arquivos disponiveis no site_teste
list:
	@echo "Arquivos em $(TEST_DIR):"
	@ls -la $(TEST_DIR)/

# Limpar arquivos compilados
clean:
	@echo "Limpando arquivos compilados..."
	@rm -f $(CLIENTE_BIN) $(SERVIDOR_BIN)

# Ajuda
help:
	@echo "Comandos disponiveis:"
	@echo "  make all     - Compila cliente e servidor"
	@echo "  make server  - Inicia o servidor com a pasta site_teste"
	@echo "  make client  - Testa o cliente baixando index.html"
	@echo "  make list    - Lista arquivos na pasta site_teste"
	@echo "  make clean   - Remove arquivos compilados"