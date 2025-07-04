#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// --- Parâmetros da Barbearia ---
#define NUM_BARBEIROS 3
#define CAPACIDADE_SOFA 4
#define LIMITE_CLIENTES_LOJA 20 // Capacidade total por segurança

// --- Estados do Cliente ---
typedef enum {
    ESPERANDO,
    CORTANDO_CABELO,
    PAGANDO,
    SAINDO
} StatusCliente;

// --- Estrutura para informações de cada cliente ---
// Cada cliente terá sua própria variável de condição para ser acordado individualmente
typedef struct {
    int id;
    StatusCliente status;
    pthread_cond_t cond_minha_vez;
} InfoCliente;

// --- Variáveis Globais (Estado Compartilhado da Loja) ---
pthread_mutex_t shop_mutex;           // Mutex para proteger o acesso às variáveis da loja
pthread_mutex_t cash_register_mutex;  // Mutex para o caixa único

pthread_cond_t cond_customer_arrived; // Barbeiros dormem nesta condição esperando por clientes

int clientes_na_loja = 0;
int clientes_no_sofa = 0;
int clientes_em_pe = 0;

// Filas para gerenciar quem está esperando
int sofa_queue[CAPACIDADE_SOFA];
int sofa_head = 0, sofa_tail = 0;

int standing_queue[LIMITE_CLIENTES_LOJA];
int standing_head = 0, standing_tail = 0;

// Informações de todos os clientes possíveis
InfoCliente* infos_clientes;

// Critério de parada
int loja_aberta = 1;
int total_clientes_a_atender = 0;
int clientes_atendidos = 0;


// --- Funções Auxiliares ---
void print_status(const char* message, int id, const char* role) {
    printf("[%-8s %2d] %s\n", role, id, message);
    fflush(stdout);
}

// --- Rotinas das Threads ---

void* rotina_barbeiro(void* arg) {
    long id_barbeiro = (long)arg;
    print_status("chegou para trabalhar.", id_barbeiro, "Barbeiro");

    while (loja_aberta || clientes_na_loja > 0) {
        pthread_mutex_lock(&shop_mutex);

        // Se não há clientes no sofá, o barbeiro dorme.
        while (clientes_no_sofa == 0 && loja_aberta) {
            print_status("está dormindo...", id_barbeiro, "Barbeiro");
            pthread_cond_wait(&cond_customer_arrived, &shop_mutex);
        }

        // Se a loja fechou e não há mais ninguém, pode ir embora.
        if (!loja_aberta && clientes_no_sofa == 0) {
            pthread_mutex_unlock(&shop_mutex);
            break;
        }

        // --- Atender um Cliente ---
        // Pega o cliente que está há mais tempo no sofá (FIFO)
        int id_cliente = sofa_queue[sofa_head];
        sofa_head = (sofa_head + 1) % CAPACIDADE_SOFA;
        clientes_no_sofa--;

        print_status("", 0, "---------------------------------");
        printf("[Barbeiro %2ld] Chamando cliente %d do sofá.\n", id_barbeiro, id_cliente);

        // Move um cliente em pé para o sofá, se houver.
        if (clientes_em_pe > 0) {
            int cliente_em_pe_id = standing_queue[standing_head];
            standing_head = (standing_head + 1) % LIMITE_CLIENTES_LOJA;
            clientes_em_pe--;

            sofa_queue[sofa_tail] = cliente_em_pe_id;
            sofa_tail = (sofa_tail + 1) % CAPACIDADE_SOFA;
            clientes_no_sofa++;
            printf("[Sistema     ] Cliente %d moveu-se da área em pé para o sofá.\n", cliente_em_pe_id);
        }
        
        // Acorda o cliente específico que será atendido
        infos_clientes[id_cliente].status = CORTANDO_CABELO;
        pthread_cond_signal(&infos_clientes[id_cliente].cond_minha_vez);

        pthread_mutex_unlock(&shop_mutex);

        // --- Cortando o Cabelo (fora do lock principal) ---
        print_status("está cortando o cabelo do cliente.", id_barbeiro, "Barbeiro");
        sleep(rand() % 3 + 2); // Simula o tempo de corte

        // --- Processo de Pagamento ---
        pthread_mutex_lock(&cash_register_mutex);
        print_status("está recebendo o pagamento do cliente.", id_barbeiro, "Barbeiro");
        sleep(rand() % 2 + 1); // Simula o tempo de pagamento
        pthread_mutex_unlock(&cash_register_mutex);

        // --- Finalização ---
        pthread_mutex_lock(&shop_mutex);
        
        print_status("terminou o serviço do cliente.", id_barbeiro, "Barbeiro");
        print_status("", 0, "---------------------------------");

        // Libera o cliente para ir embora
        infos_clientes[id_cliente].status = SAINDO;
        pthread_cond_signal(&infos_clientes[id_cliente].cond_minha_vez);
        
        clientes_atendidos++;
        pthread_mutex_unlock(&shop_mutex);
    }

    print_status("foi para casa.", id_barbeiro, "Barbeiro");
    return NULL;
}

void* rotina_cliente(void* arg) {
    long id_cliente = (long)arg;
    usleep((rand() % 500) * 1000); // Chegada em tempos variados

    pthread_mutex_lock(&shop_mutex);
    print_status("chegou na barbearia.", id_cliente, "Cliente");

    // Verifica se a loja está cheia
    if (clientes_na_loja >= LIMITE_CLIENTES_LOJA) {
        print_status("viu a loja cheia e foi embora.", id_cliente, "Cliente");
        pthread_mutex_unlock(&shop_mutex);
        return NULL;
    }

    clientes_na_loja++;

    // Tenta sentar no sofá
    if (clientes_no_sofa < CAPACIDADE_SOFA) {
        sofa_queue[sofa_tail] = id_cliente;
        sofa_tail = (sofa_tail + 1) % CAPACIDADE_SOFA;
        clientes_no_sofa++;
        print_status("sentou no sofá.", id_cliente, "Cliente");
        
        // Acorda um barbeiro se houver algum dormindo
        pthread_cond_signal(&cond_customer_arrived);

    } else { // Se o sofá está cheio, fica em pé
        standing_queue[standing_tail] = id_cliente;
        standing_tail = (standing_tail + 1) % LIMITE_CLIENTES_LOJA;
        clientes_em_pe++;
        print_status("está esperando em pé.", id_cliente, "Cliente");
    }

    // Espera até ser chamado, ter o cabelo cortado e pagar
    while (infos_clientes[id_cliente].status != SAINDO) {
        pthread_cond_wait(&infos_clientes[id_cliente].cond_minha_vez, &shop_mutex);
        
        if(infos_clientes[id_cliente].status == CORTANDO_CABELO) {
             print_status("está tendo o cabelo cortado.", id_cliente, "Cliente");
        }
    }

    clientes_na_loja--;
    print_status("pagou e foi embora satisfeito.", id_cliente, "Cliente");

    pthread_mutex_unlock(&shop_mutex);
    return NULL;
}


// --- Função Principal ---
int main() {
    printf("Digite o número total de clientes que a barbearia atenderá hoje: ");
    scanf("%d", &total_clientes_a_atender);

    if (total_clientes_a_atender <= 0) {
        printf("Número inválido de clientes. Saindo.\n");
        return 1;
    }

    pthread_t barbeiros[NUM_BARBEIROS];
    pthread_t* clientes = malloc(total_clientes_a_atender * sizeof(pthread_t));
    infos_clientes = malloc(total_clientes_a_atender * sizeof(InfoCliente));

    srand(time(NULL));

    // Inicialização dos mutexes e variáveis de condição
    pthread_mutex_init(&shop_mutex, NULL);
    pthread_mutex_init(&cash_register_mutex, NULL);
    pthread_cond_init(&cond_customer_arrived, NULL);

    for (int i = 0; i < total_clientes_a_atender; i++) {
        infos_clientes[i].id = i;
        infos_clientes[i].status = ESPERANDO;
        pthread_cond_init(&infos_clientes[i].cond_minha_vez, NULL);
    }
    
    printf("\n=== A Barbearia Abriu! ===\n\n");

    // Criando as threads dos barbeiros
    for (long i = 0; i < NUM_BARBEIROS; i++) {
        pthread_create(&barbeiros[i], NULL, rotina_barbeiro, (void*)i);
    }

    // Criando as threads dos clientes
    for (long i = 0; i < total_clientes_a_atender; i++) {
        pthread_create(&clientes[i], NULL, rotina_cliente, (void*)i);
        usleep((rand() % 200) * 1000); // Simula um intervalo entre a chegada dos clientes
    }

    // Espera todas as threads de clientes terminarem
    for (int i = 0; i < total_clientes_a_atender; i++) {
        pthread_join(clientes[i], NULL);
    }
    
    printf("\n=== Todos os clientes do dia foram atendidos. Fechando a loja... ===\n\n");
    
    // Critério de parada: quando todos os clientes foram atendidos, a loja fecha.
    loja_aberta = 0;
    
    // Acorda todos os barbeiros que possam estar dormindo para que eles possam sair do loop
    pthread_cond_broadcast(&cond_customer_arrived);

    // Espera as threads dos barbeiros terminarem
    for (int i = 0; i < NUM_BARBEIROS; i++) {
        pthread_join(barbeiros[i], NULL);
    }

    printf("\n=== A Barbearia Fechou! ===\n");

    // Liberação de recursos
    pthread_mutex_destroy(&shop_mutex);
    pthread_mutex_destroy(&cash_register_mutex);
    pthread_cond_destroy(&cond_customer_arrived);
    for (int i = 0; i < total_clientes_a_atender; i++) {
        pthread_cond_destroy(&infos_clientes[i].cond_minha_vez);
    }
    free(clientes);
    free(infos_clientes);

    return 0;
}