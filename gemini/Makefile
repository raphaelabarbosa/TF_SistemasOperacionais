# Makefile para o problema da Barbearia

# Compilador
CC = gcc

# Flags do compilador:
# -Wall e -Wextra: Habilita a maioria dos avisos úteis.
# -g: Adiciona símbolos de depuração.
# -pthread: Essencial para linkar a biblioteca pthreads.
CFLAGS = -Wall -Wextra -g -pthread

# O nome do arquivo de saída (executável)
TARGET = barbearia

# O nome do arquivo fonte
SRCS = barbearia.c

# A regra padrão, que é executada quando você digita 'make'
all: $(TARGET)

# Regra para construir o executável
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)
	@echo "Compilação concluída! Execute com ./${TARGET}"

# Regra para limpar os arquivos gerados
clean:
	@echo "Limpando arquivos compilados..."
	rm -f $(TARGET)

# Regra para compilar e executar o programa
run: all
	@echo "Executando o programa..."
	./$(TARGET)

# Phony targets não representam arquivos
.PHONY: all clean run