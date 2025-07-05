/*
 * barbershop_concurrency.c
 * Simulação de barbearia com vários barbeiros usando pthreads, locks e variáveis de condição.
 * Parâmetros são lidos via entrada interativa.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <unistd.h>
 #include <time.h>
 
 // Parâmetros da barbearia
 int num_barbers, sofa_capacity, shop_capacity, total_customers;
 
 // Contadores internos
 int sofa_spots_available;
 int waiting_standing = 0;
 int customers_in_shop = 0;
 int customers_served = 0;
 
 // Filas para controlar IDs de clientes
 int *sofa_queue;
 int sofa_head = 0, sofa_tail = 0, sofa_count = 0;
 
 // Condições para comunicação específica com clientes
 pthread_cond_t *haircut_cond;
 pthread_cond_t *payment_cond;
 
 // Mutexes e variáveis de condição gerais
 pthread_mutex_t shop_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t pay_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t sofa_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t barber_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t customer_ready = PTHREAD_COND_INITIALIZER;
 
 // Enfileira cliente na fila do sofá
 void enqueue_sofa(int client_id) {
     sofa_queue[sofa_tail] = client_id;
     sofa_tail = (sofa_tail + 1) % total_customers;
     sofa_count++;
 }
 
 // Desenfileira cliente da fila do sofá
 int dequeue_sofa() {
     int client_id = sofa_queue[sofa_head];
     sofa_head = (sofa_head + 1) % total_customers;
     sofa_count--;
     return client_id;
 }
 
 void* barber_thread(void* arg) {
     int id = *(int*)arg;
     free(arg);
 
     while (1) {
         pthread_mutex_lock(&shop_mutex);
         while (sofa_count == 0 && customers_served < total_customers) {
             printf("Barbeiro %d dormindo (sem clientes)\n", id);
             pthread_cond_wait(&customer_ready, &shop_mutex);
         }
         if (customers_served >= total_customers && sofa_count == 0) {
             pthread_mutex_unlock(&shop_mutex);
             break;
         }
         // Atende cliente do sofá
         int client_id = dequeue_sofa();
         sofa_spots_available++;
         printf("Barbeiro %d inicia corte do cliente %d. Lugares no sofá livres: %d\n", id, client_id, sofa_spots_available);
         // Notifica cliente que corte vai começar
         pthread_cond_signal(&haircut_cond[client_id]);
 
         if (waiting_standing > 0) pthread_cond_signal(&sofa_cond);
         pthread_mutex_unlock(&shop_mutex);
 
         // Simula corte de cabelo
         int duration = rand() % 3 + 1;
         printf("Barbeiro %d cortando cabelo do cliente %d por %d segundos\n", id, client_id, duration);
         sleep(duration);
         printf("Barbeiro %d terminou corte do cliente %d\n", id, client_id);
 
         // Pagamento (registradora única)
         pthread_mutex_lock(&pay_mutex);
         printf("Barbeiro %d aceitando pagamento do cliente %d\n", id, client_id);
         pthread_mutex_lock(&shop_mutex);
         // Notifica cliente que pagamento vai acontecer
         pthread_cond_signal(&payment_cond[client_id]);
         pthread_mutex_unlock(&shop_mutex);
         sleep(1);
         printf("Barbeiro %d terminou pagamento do cliente %d\n", id, client_id);
         pthread_mutex_unlock(&pay_mutex);
 
         // Cliente sai
         pthread_mutex_lock(&shop_mutex);
         customers_served++;
         customers_in_shop--;
         printf("Cliente %d saiu. Total servidos: %d, clientes na loja: %d\n", client_id, customers_served, customers_in_shop);
         pthread_cond_broadcast(&barber_cond);
         pthread_cond_broadcast(&customer_ready);
         pthread_mutex_unlock(&shop_mutex);
     }
     printf("Barbeiro %d encerrando turno.\n", id);
     return NULL;
 }
 
 void* customer_thread(void* arg) {
     int id = *(int*)arg;
     free(arg);
 
     pthread_mutex_lock(&shop_mutex);
     while (customers_in_shop >= shop_capacity)
         pthread_cond_wait(&barber_cond, &shop_mutex);
     customers_in_shop++;
     printf("Cliente %d entrou. Clientes presentes: %d\n", id, customers_in_shop);
     pthread_mutex_unlock(&shop_mutex);
 
     pthread_mutex_lock(&shop_mutex);
     if (sofa_spots_available > 0) {
         sofa_spots_available--;
         enqueue_sofa(id);
         printf("Cliente %d sentou no sofá. Lugares no sofá livres: %d\n", id, sofa_spots_available);
     } else {
         waiting_standing++;
         printf("Cliente %d ficou em pé. Lugares em pé: %d\n", id, waiting_standing);
         while (sofa_spots_available == 0)
             pthread_cond_wait(&sofa_cond, &shop_mutex);
         waiting_standing--;
         sofa_spots_available--;
         enqueue_sofa(id);
         printf("Cliente %d moveu-se para o sofá. Lugares em pé: %d, lugares no sofá livres: %d\n", id, waiting_standing, sofa_spots_available);
     }
     pthread_cond_signal(&customer_ready);
     pthread_mutex_unlock(&shop_mutex);
 
     // Aguarda início do corte
     pthread_mutex_lock(&shop_mutex);
     pthread_cond_wait(&haircut_cond[id], &shop_mutex);
     printf("Cliente %d: meu corte começou\n", id);
     pthread_mutex_unlock(&shop_mutex);
 
     // Aguarda pagamento
     pthread_mutex_lock(&shop_mutex);
     pthread_cond_wait(&payment_cond[id], &shop_mutex);
     printf("Cliente %d: estou realizando pagamento\n", id);
     pthread_mutex_unlock(&shop_mutex);
 
     return NULL;
 }
 
 int main() {
     srand(time(NULL));
 
     printf("Número de barbeiros: "); scanf("%d", &num_barbers);
     printf("Capacidade do sofá: "); scanf("%d", &sofa_capacity);
     printf("Capacidade máxima da loja: "); scanf("%d", &shop_capacity);
     printf("Total de clientes previstos: "); scanf("%d", &total_customers);
 
     sofa_spots_available = sofa_capacity;
     sofa_queue = malloc(sizeof(int) * total_customers);
     haircut_cond = malloc(sizeof(pthread_cond_t) * (total_customers + 1));
     payment_cond = malloc(sizeof(pthread_cond_t) * (total_customers + 1));
 
     for (int i = 0; i <= total_customers; i++) {
         pthread_cond_init(&haircut_cond[i], NULL);
         pthread_cond_init(&payment_cond[i], NULL);
     }
 
     pthread_t *barbers = malloc(sizeof(pthread_t) * num_barbers);
     pthread_t *customers = malloc(sizeof(pthread_t) * total_customers);
 
     for (int i = 0; i < num_barbers; i++) {
         int *id = malloc(sizeof(int)); *id = i + 1;
         pthread_create(&barbers[i], NULL, barber_thread, id);
     }
     for (int i = 0; i < total_customers; i++) {
         int *id = malloc(sizeof(int)); *id = i + 1;
         pthread_create(&customers[i], NULL, customer_thread, id);
         sleep(rand() % 2 + 1);
     }
 
     for (int i = 0; i < num_barbers; i++) pthread_join(barbers[i], NULL);
 
     free(sofa_queue);
     free(haircut_cond);
     free(payment_cond);
     free(barbers);
     free(customers);
     printf("Simulação encerrada.\n");
     return 0;
 }