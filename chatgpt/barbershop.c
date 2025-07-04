#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define MAX_THREADS 100

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t customer_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t payment_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t receipt_given = PTHREAD_COND_INITIALIZER;

int customers_in_shop = 0;
int sofa_customers = 0;
int standing_customers = 0;
bool paying = false;

int total_customers;
int num_barbers;
int sofa_capacity;
int max_customers_in_shop;

void* barber(void* arg) {
    int id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&mutex);
        while (sofa_customers == 0 && customers_in_shop == 0) {
            printf("Barbeiro %d dormindo.\n", id);
            pthread_cond_wait(&customer_ready, &mutex);
        }

        if (sofa_customers > 0) {
            sofa_customers--;
            printf("Barbeiro %d está cortando cabelo. Pessoas no sofá: %d\n", id, sofa_customers);
            pthread_mutex_unlock(&mutex);
            sleep(rand() % 3 + 1); // simula o corte de cabelo

            pthread_mutex_lock(&mutex);
            while (paying) {
                pthread_cond_wait(&payment_ready, &mutex);
            }
            paying = true;
            printf("Barbeiro %d está recebendo pagamento.\n", id);
            pthread_mutex_unlock(&mutex);

            sleep(1); // simula tempo de pagamento

            pthread_mutex_lock(&mutex);
            paying = false;
            pthread_cond_signal(&payment_ready);
            pthread_cond_signal(&receipt_given);
            pthread_mutex_unlock(&mutex);
        } else {
            pthread_mutex_unlock(&mutex);
        }
    }
    return NULL;
}

void* customer(void* arg) {
    int id = *(int*)arg;

    pthread_mutex_lock(&mutex);
    if (customers_in_shop >= max_customers_in_shop) {
        printf("Cliente %d não entra - salão cheio.\n", id);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    customers_in_shop++;
    if (sofa_customers < sofa_capacity) {
        sofa_customers++;
        printf("Cliente %d senta no sofá. Total no salão: %d\n", id, customers_in_shop);
    } else {
        standing_customers++;
        printf("Cliente %d está em pé. Total no salão: %d\n", id, customers_in_shop);
    }
    pthread_cond_signal(&customer_ready);
    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3 + 1); // aguarda atendimento

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&receipt_given, &mutex);
    customers_in_shop--;
    if (standing_customers > 0 && sofa_customers < sofa_capacity) {
        standing_customers--;
        sofa_customers++;
        printf("Um cliente em pé senta no sofá. Pessoas no sofá: %d\n", sofa_customers);
    }
    printf("Cliente %d saiu do salão. Total agora: %d\n", id, customers_in_shop);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* entrada_clientes(void* arg) {
    int* total = (int*)arg;
    pthread_t customers[*total];
    int customer_ids[*total];

    for (int i = 0; i < *total; i++) {
        customer_ids[i] = i + 1;
        pthread_create(&customers[i], NULL, customer, &customer_ids[i]);
        sleep(rand() % 3 + 1); // intervalo aleatório de chegada
    }

    for (int i = 0; i < *total; i++) {
        pthread_join(customers[i], NULL);
    }

    printf("Todos os clientes foram atendidos. Encerrando o programa.\n");
    exit(0);
}

int main() {
    srand(time(NULL));

    printf("=== CONFIGURAÇÃO DA BARBEARIA ===\n");
    printf("Digite o número de clientes: ");
    scanf("%d", &total_customers);
    printf("Digite o número de barbeiros: ");
    scanf("%d", &num_barbers);
    printf("Digite a capacidade do sofá: ");
    scanf("%d", &sofa_capacity);
    printf("Digite o número máximo de clientes no salão (inclui sofá e em pé): ");
    scanf("%d", &max_customers_in_shop);

    if (total_customers > MAX_THREADS) total_customers = MAX_THREADS;

    pthread_t barbers[num_barbers];
    int barber_ids[num_barbers];

    for (int i = 0; i < num_barbers; i++) {
        barber_ids[i] = i + 1;
        pthread_create(&barbers[i], NULL, barber, &barber_ids[i]);
    }

    pthread_t entrada_thread;
    pthread_create(&entrada_thread, NULL, entrada_clientes, &total_customers);
    pthread_join(entrada_thread, NULL);

    return 0;
}
