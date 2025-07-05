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
 
 // Mutexes e variáveis de condição
 pthread_mutex_t shop_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t pay_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t sofa_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t barber_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t customer_ready = PTHREAD_COND_INITIALIZER;
 
 void* barber_thread(void* arg) {
     int id = *(int*)arg;
     free(arg);
 
     while (1) {
         pthread_mutex_lock(&shop_mutex);
         // Aguardar cliente no sofá ou fim da simulação
         while (sofa_spots_available == sofa_capacity && customers_served < total_customers) {
             printf("Barbeiro %d dormindo (sem clientes)\n", id);
             pthread_cond_wait(&customer_ready, &shop_mutex);
         }
         // Se todos clientes atendidos, sai
         if (customers_served >= total_customers) {
             pthread_mutex_unlock(&shop_mutex);
             break;
         }
         // Atende cliente do sofá
         sofa_spots_available++;
         printf("Barbeiro %d atendendo cliente do sofá. Lugares no sofá livres: %d\n", id, sofa_spots_available);
         // Se houver clientes em pé, um ocupa o sofá
         if (waiting_standing > 0) {
             waiting_standing--;
             sofa_spots_available--;
             printf("Cliente em pé ocupa o sofá. Lugares em pé: %d, lugares no sofá livres: %d\n", waiting_standing, sofa_spots_available);
             pthread_cond_signal(&sofa_cond);
         }
         pthread_mutex_unlock(&shop_mutex);
 
         // Simula corte de cabelo
         int duration = rand() % 3 + 1;
         printf("Barbeiro %d cortando cabelo por %d segundos\n", id, duration);
         sleep(duration);
 
         // Pagamento (registradora única)
         pthread_mutex_lock(&pay_mutex);
         printf("Barbeiro %d aceitando pagamento\n", id);
         sleep(1);
         printf("Barbeiro %d terminou pagamento\n", id);
         pthread_mutex_unlock(&pay_mutex);
 
         // Cliente sai da barbearia
         pthread_mutex_lock(&shop_mutex);
         customers_served++;
         customers_in_shop--;
         printf("Cliente saiu. Total servidos: %d, clientes na loja: %d\n", customers_served, customers_in_shop);
         // Notifica clientes na porta e barbearia vazia
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
 
     // Tenta entrar na barbearia
     pthread_mutex_lock(&shop_mutex);
     while (customers_in_shop >= shop_capacity) {
         printf("Cliente %d esperando fora (loja cheia)\n", id);
         pthread_cond_wait(&barber_cond, &shop_mutex);
     }
     customers_in_shop++;
     printf("Cliente %d entrou na loja. Clientes presentes: %d\n", id, customers_in_shop);
     pthread_mutex_unlock(&shop_mutex);
 
     // Tenta sentar no sofá
     pthread_mutex_lock(&shop_mutex);
     if (sofa_spots_available > 0) {
         sofa_spots_available--;
         printf("Cliente %d sentou no sofá. Lugares no sofá livres: %d\n", id, sofa_spots_available);
     } else {
         waiting_standing++;
         printf("Cliente %d ficou em pé. Lugares em pé: %d\n", id, waiting_standing);
         while (sofa_spots_available == 0) {
             pthread_cond_wait(&sofa_cond, &shop_mutex);
         }
         waiting_standing--;
         sofa_spots_available--;
         printf("Cliente %d moveu-se para o sofá. Lugares em pé: %d, lugares no sofá livres: %d\n", id, waiting_standing, sofa_spots_available);
     }
     // Notifica barbeiros de cliente pronto
     pthread_cond_signal(&customer_ready);
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
 
     pthread_t* barbers = malloc(sizeof(pthread_t) * num_barbers);
     pthread_t* customers = malloc(sizeof(pthread_t) * total_customers);
 
     // Cria threads de barbeiros
     for (int i = 0; i < num_barbers; i++) {
         int* id = malloc(sizeof(int)); *id = i+1;
         pthread_create(&barbers[i], NULL, barber_thread, id);
     }
     // Cria threads de clientes
     for (int i = 0; i < total_customers; i++) {
         int* id = malloc(sizeof(int)); *id = i+1;
         pthread_create(&customers[i], NULL, customer_thread, id);
         sleep(rand() % 2 + 1); // chegada randômica
     }
 
     // Aguarda barbeiros terminarem
     for (int i = 0; i < num_barbers; i++) {
         pthread_join(barbers[i], NULL);
     }
 
     free(barbers);
     free(customers);
     printf("Simulação encerrada.\n");
     return 0;
 }