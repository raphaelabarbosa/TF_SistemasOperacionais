#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// ---- Parâmetros da Barbearia (agora são variáveis globais) ----
int NUM_BARBEIROS;
int CAPACIDADE_SOFA;
int LIMITE_CLIENTES_LOJA;
int CAPACIDADE_EM_PE;       // CORREÇÃO: Capacidade explícita para a área em pé
int total_clientes_a_gerar;
int tempo_base_ms;

// --- Estados do Cliente ---
typedef enum {
    ESPERANDO,
    CORTANDO_CABELO,
    SAINDO
} StatusCliente;

// --- Estrutura para informações de cada cliente ---
typedef struct {
    int id;
    StatusCliente status;
    pthread_cond_t cond_minha_vez;
} InfoCliente;

// --- Variáveis Globais (Estado Compartilhado da Loja) ---
pthread_mutex_t shop_mutex;
pthread_mutex_t cash_register_mutex;
pthread_cond_t cond_customer_arrived;

int clientes_na_loja = 0;
int clientes_no_sofa = 0;
int clientes_em_pe = 0;

int* sofa_queue;
int sofa_head = 0, sofa_tail = 0;

int* standing_queue;
int standing_head = 0, standing_tail = 0;

InfoCliente* infos_clientes;
pthread_t* threads_clientes;

int loja_aberta = 1;

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

        while (clientes_no_sofa == 0 && loja_aberta) {
            print_status("está dormindo...", id_barbeiro, "Barbeiro");
            pthread_cond_wait(&cond_customer_arrived, &shop_mutex);
        }

        if (!loja_aberta && clientes_no_sofa == 0) {
            pthread_mutex_unlock(&shop_mutex);
            break;
        }

        int id_cliente = sofa_queue[sofa_head];
        sofa_head = (sofa_head + 1) % CAPACIDADE_SOFA;
        clientes_no_sofa--;

        print_status("", 0, "---------------------------------");
        printf("[Barbeiro %2ld] Chamando cliente %d do sofá.\n", id_barbeiro, id_cliente);

        if (clientes_em_pe > 0) {
            int cliente_em_pe_id = standing_queue[standing_head];
            // CORREÇÃO: Usar a capacidade correta da área em pé para o módulo
            standing_head = (standing_head + 1) % CAPACIDADE_EM_PE;
            clientes_em_pe--;

            sofa_queue[sofa_tail] = cliente_em_pe_id;
            sofa_tail = (sofa_tail + 1) % CAPACIDADE_SOFA;
            clientes_no_sofa++;
            printf("[Sistema     ] Cliente %d moveu-se da área em pé para o sofá.\n", cliente_em_pe_id);
        }

        infos_clientes[id_cliente].status = CORTANDO_CABELO;
        pthread_cond_signal(&infos_clientes[id_cliente].cond_minha_vez);
        pthread_mutex_unlock(&shop_mutex);

        char msg[100];
        sprintf(msg, "está cortando o cabelo do cliente %d.", id_cliente);
        print_status(msg, id_barbeiro, "Barbeiro");
        usleep(((rand() % 3 + 2) * 1000) * tempo_base_ms);

        pthread_mutex_lock(&cash_register_mutex);
        sprintf(msg, "está recebendo o pagamento do cliente %d.", id_cliente);
        print_status(msg, id_barbeiro, "Barbeiro");
        usleep(((rand() % 2 + 1) * 1000) * tempo_base_ms);
        pthread_mutex_unlock(&cash_register_mutex);

        pthread_mutex_lock(&shop_mutex);
        sprintf(msg, "terminou o serviço do cliente %d.", id_cliente);
        print_status(msg, id_barbeiro, "Barbeiro");
        print_status("", 0, "---------------------------------");

        infos_clientes[id_cliente].status = SAINDO;
        pthread_cond_signal(&infos_clientes[id_cliente].cond_minha_vez);
        pthread_mutex_unlock(&shop_mutex);
    }

    print_status("foi para casa.", id_barbeiro, "Barbeiro");
    return NULL;
}

void* rotina_cliente(void* arg) {
    long id_cliente = (long)arg;

    pthread_mutex_lock(&shop_mutex);
    print_status("chegou na barbearia.", id_cliente, "Cliente");

    if (clientes_na_loja >= LIMITE_CLIENTES_LOJA) {
        print_status("viu a loja cheia e foi embora.", id_cliente, "Cliente");
        pthread_mutex_unlock(&shop_mutex);
        return NULL;
    }

    clientes_na_loja++;

    // CORREÇÃO: Lógica explícita para checar sofá e depois área em pé
    if (clientes_no_sofa < CAPACIDADE_SOFA) {
        sofa_queue[sofa_tail] = id_cliente;
        sofa_tail = (sofa_tail + 1) % CAPACIDADE_SOFA;
        clientes_no_sofa++;
        print_status("sentou no sofá.", id_cliente, "Cliente");
        pthread_cond_signal(&cond_customer_arrived);
    } else if (clientes_em_pe < CAPACIDADE_EM_PE) {
        standing_queue[standing_tail] = id_cliente;
        // CORREÇÃO: Usar a capacidade correta da área em pé para o módulo
        standing_tail = (standing_tail + 1) % CAPACIDADE_EM_PE;
        clientes_em_pe++;
        print_status("está esperando em pé.", id_cliente, "Cliente");
    }

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

void* gerador_clientes(void* arg) {
    for (long i = 0; i < total_clientes_a_gerar; i++) {
        usleep(((500 + rand() % 1500)) * tempo_base_ms);
        pthread_create(&threads_clientes[i], NULL, rotina_cliente, (void*)i);
    }
    return NULL;
}

int main() {
    printf("--- Configuração da Barbearia ---\n");
    printf("Digite o número de Barbeiros: ");
    scanf("%d", &NUM_BARBEIROS);
    printf("Digite a capacidade do sofá: ");
    scanf("%d", &CAPACIDADE_SOFA);
    printf("Digite o limite total de clientes na loja (sofá + em pé): ");
    scanf("%d", &LIMITE_CLIENTES_LOJA);
    printf("Digite o número total de clientes a serem gerados no dia: ");
    scanf("%d", &total_clientes_a_gerar);
    printf("Digite o tempo base da simulação em ms (ex: 100 para rápido, 1000 para normal): ");
    scanf("%d", &tempo_base_ms);

    if (NUM_BARBEIROS <= 0 || CAPACIDADE_SOFA <= 0 || LIMITE_CLIENTES_LOJA <= 0 || total_clientes_a_gerar <= 0 || tempo_base_ms <=0) {
        printf("Parâmetros inválidos. Todos os valores devem ser maiores que zero.\n");
        return 1;
    }
    if (CAPACIDADE_SOFA > LIMITE_CLIENTES_LOJA) {
        printf("A capacidade do sofá não pode ser maior que o limite total da loja.\n");
        return 1;
    }

    // CORREÇÃO: Calcular a capacidade da área em pé
    CAPACIDADE_EM_PE = LIMITE_CLIENTES_LOJA - CAPACIDADE_SOFA;

    pthread_t barbeiros[NUM_BARBEIROS];
    threads_clientes = malloc(total_clientes_a_gerar * sizeof(pthread_t));
    infos_clientes = malloc(total_clientes_a_gerar * sizeof(InfoCliente));
    sofa_queue = malloc(CAPACIDADE_SOFA * sizeof(int));
    // CORREÇÃO: Alocar memória com o tamanho correto
    if (CAPACIDADE_EM_PE > 0) {
        standing_queue = malloc(CAPACIDADE_EM_PE * sizeof(int));
    } else {
        standing_queue = NULL;
    }

    srand(time(NULL));

    pthread_mutex_init(&shop_mutex, NULL);
    pthread_mutex_init(&cash_register_mutex, NULL);
    pthread_cond_init(&cond_customer_arrived, NULL);

    for (int i = 0; i < total_clientes_a_gerar; i++) {
        infos_clientes[i].id = i;
        infos_clientes[i].status = ESPERANDO;
        pthread_cond_init(&infos_clientes[i].cond_minha_vez, NULL);
    }

    printf("\n======================================================\n");
    printf("=== A Barbearia Abriu! ===\n");
    printf("Configuração: %d Barbeiros, %d lugares no sofá, %d lugares em pé.\n",
           NUM_BARBEIROS, CAPACIDADE_SOFA, CAPACIDADE_EM_PE); // CORREÇÃO: Usar a nova variável
    printf("Total de clientes hoje: %d\n", total_clientes_a_gerar);
    printf("======================================================\n\n");

    for (long i = 0; i < NUM_BARBEIROS; i++) {
        pthread_create(&barbeiros[i], NULL, rotina_barbeiro, (void*)i);
    }

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, gerador_clientes, NULL);

    pthread_join(generator_thread, NULL);
    printf("\n=== O gerador de clientes terminou. A porta da loja não aceita mais ninguém. ===\n\n");

    for (int i = 0; i < total_clientes_a_gerar; i++) {
        pthread_join(threads_clientes[i], NULL);
    }

    printf("\n=== Todos os clientes do dia foram embora. Fechando a loja... ===\n\n");

    pthread_mutex_lock(&shop_mutex);
    loja_aberta = 0;
    pthread_mutex_unlock(&shop_mutex);
    
    pthread_cond_broadcast(&cond_customer_arrived);

    for (int i = 0; i < NUM_BARBEIROS; i++) {
        pthread_join(barbeiros[i], NULL);
    }

    printf("\n=== A Barbearia Fechou! ===\n");

    pthread_mutex_destroy(&shop_mutex);
    pthread_mutex_destroy(&cash_register_mutex);
    pthread_cond_destroy(&cond_customer_arrived);
    for (int i = 0; i < total_clientes_a_gerar; i++) {
        pthread_cond_destroy(&infos_clientes[i].cond_minha_vez);
    }
    free(threads_clientes);
    free(infos_clientes);
    free(sofa_queue);
    if (standing_queue != NULL) {
        free(standing_queue);
    }

    return 0;
}