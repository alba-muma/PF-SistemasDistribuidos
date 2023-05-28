#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
int sd, err ,val , sc;
pthread_mutex_t mutex_mensaje;
int mensaje_no_copiado = true;
pthread_cond_t cond_mensaje;
int32_t operacion_s;
int contador_id = 0;
// Estructura con atributos del mensaje
struct mensaje{
    char msg[256];
    char emisor[256];
    char id[256];
};

// Estructura con atributos para los usuarios
struct usuario{
    int socketid;
    struct sockaddr_in address;
    char username[256];
    char alias[256];
    char date[256];
    bool isConnected; // Booleano que indica si el usuario esta o no conectado
    char puerto[6];
    char ip[256]; // Puerto en el que se enviaran los mensajes
    int n_mensajes; // Numero de mensajes
    struct mensaje *mensajes; // Lista de mensajes del usuario
};

// Parametros que se envian a la funcion enviar_mensajes
struct param_send{
    char receptor[256];
    char emisor[256];
    char mensaje_enviado[256];
};

// Creamos el "almacen" de usuarios
struct usuario *usuarios;
int n_usuarios = 0;
pthread_mutex_t usuariosMutex;

void registrar (void *user) {
    // Funcion que registra al usuario 
    struct usuario usuario_register;
    pthread_mutex_lock(&mutex_mensaje);
    usuario_register = (*(struct usuario *)user);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    char resultado = '0';
    pthread_mutex_lock(&usuariosMutex);
    // Buscamos si el usuario esta registrado
    for (int i = 0; i < n_usuarios; i++){
        if (strcmp(usuario_register.alias, usuarios[i].alias) == 0) {
            // El usuarios está registrado
            resultado = '1';
            printf("REGISTER %s FAIL\n", usuario_register.alias);
        }
    }
    if (resultado == '0'){ // Si no esta registrado
        n_usuarios += 1;
        if (n_usuarios == 1) { // Sino hay usuarios se hará un malloc para solo un usuarios
            usuarios = malloc(sizeof(struct usuario));
        } else { // Si ya hay usuarios se modifica el tamaño de usuarios
            usuarios = realloc(usuarios, (n_usuarios) * sizeof(struct usuario));
        }
        // Añadimos al usuarios a la lista de usuarios
        usuario_register.isConnected = false;
        usuario_register.n_mensajes = 0;
        strcpy(usuario_register.puerto , "-1");
        usuarios[n_usuarios - 1] = usuario_register;
        printf("REGISTER %s OK\n", usuario_register.alias);
    }
    // Enviamos el resultado de la operacion al cliente
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    pthread_mutex_unlock(&usuariosMutex);
}

void deregistrar (void *user){
    // Funcion que deregistra a un usuario registrado
    struct usuario usuario_unregister;
    int pos = -1;
    pthread_mutex_lock(&mutex_mensaje);
    usuario_unregister = (*(struct usuario *)user);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    char resultado = '0';
    pthread_mutex_lock(&usuariosMutex);
    // Buscamos si el usuario esta registrado
    for (int i = 0; i < n_usuarios; i++){
        if (strcmp(usuario_unregister.alias, usuarios[i].alias) == 0) {
            // El usuario encontrado
            pos = i;
        }
    }
    // Si no estaba registrado error
    if (pos == -1){
        resultado = '1';
        printf("UNREGISTER %s FAIL\n", usuario_unregister.alias);
    }
    else{
        // Si estaba registrado lo eliminamos
        for (int i = pos; i < n_usuarios - 1; i++) {
            // Apartir de la posicion del usuario a eliminar movemos todos los usuarios
            usuarios[i] = usuarios[i + 1];
        }
        n_usuarios--;
        // Se hace un realloc poruqe ahora hay un usuario menos
        usuarios = realloc(usuarios, (n_usuarios) * sizeof(struct usuario));
        resultado = '0';
        printf("UNREGISTER %s OK\n", usuario_unregister.alias);
    }
    // Enviamos el resultado al cliente
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    pthread_mutex_unlock(&usuariosMutex);

}

void conectar (void *user){
    // Funcion que conecta a un usuario
    struct usuario usuario_conectar;
    pthread_mutex_lock(&mutex_mensaje);
    usuario_conectar = (*(struct usuario *)user);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    char resultado = '1';
    pthread_mutex_lock(&usuariosMutex);
    int pos_user;
    for (int i = 0; i < n_usuarios; i++){
        if (strcmp(usuario_conectar.alias, usuarios[i].alias) == 0) {
            if (usuarios[i].isConnected == true){
                // Si el usuario ya esta conectado manda un 2
                resultado = '2';
                printf("CONNECT %s FAIL\n", usuarios[i].alias);
            }
            else{
                // Si el usuario no esta conectado cambia su estado y envia un 0
                usuarios[i].isConnected = true;
                strcpy(usuarios[i].puerto ,usuario_conectar.puerto);
                strcpy(usuarios[i].ip, usuario_conectar.ip);
                resultado = '0';
                printf("CONNECT %s OK\n", usuarios[i].alias);
                
            }
            pos_user = i;
        }
    }
    // Enviamos al cliente el resultado de la operacion
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    sleep(1.5);
    // Cuando se conecta un usuario se mira si tienen mensajes en cola
    if (usuarios[pos_user].n_mensajes > 0){
        // Se recorren todos los mensajes y se envian por el socket con el puerto correspondiente al usuario
        for(int x = 0; x < usuarios[pos_user].n_mensajes; x++){
            // Se crea el socket y se conecta al puerto
            int new_socket_c = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in server_addr_c;
            server_addr_c.sin_family = AF_INET;
            server_addr_c.sin_port = htons(atoi(usuarios[pos_user].puerto)); 
            server_addr_c.sin_addr.s_addr = INADDR_ANY;
            if (connect(new_socket_c, (struct sockaddr *)&server_addr_c, sizeof(server_addr_c)) == -1) {
                printf("Error en la conexión\n");
                close(new_socket_c);
            }
            // Se copian las dos operaciones posibles (indicar que ha llegado el mensaje , recibir mensaje)
            char op1[256];
            memset(op1,'\0',sizeof(op1));
            strcpy(op1, "SEND_MESSAGE");
            char op2[256];
            memset(op2,'\0',sizeof(op1));
            strcpy(op2, "SEND_MESS_ACK");
            // Se envia la operacion para enviar el mensajes
            if (send(new_socket_c, &op1, sizeof(op1), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket_c);
            }
            char emisor[256];
            memset(emisor, '\0', sizeof(emisor));
            strcpy(emisor, usuarios[pos_user].mensajes[x].emisor);
            // Se envia el emisor
            if (send(new_socket_c, &emisor, sizeof(emisor), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket_c);
            }
            char id_mensaje[256];
            memset(id_mensaje, '\0', sizeof(id_mensaje));
            strcpy(id_mensaje, usuarios[pos_user].mensajes[x].id);
            // Se envia la id del mensaje
            if (send(new_socket_c, id_mensaje, sizeof(id_mensaje), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket_c);
            }
            char msg_enviado[256];
            memset(msg_enviado, '\0', sizeof(msg_enviado));
            strcpy(msg_enviado , usuarios[pos_user].mensajes[x].msg);
            // Se envia el mensaje
            if (send(new_socket_c, &msg_enviado, sizeof(msg_enviado), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket_c);
            }
            printf("SEND MESSAGE %s FROM %s TO %s\n", id_mensaje, emisor, usuarios[pos_user].alias);
            for (int n = 0; n < n_usuarios; n++){
                // Se busca al usuario emisor
                if (strcmp(usuarios[n].alias, usuarios[pos_user].mensajes[x].emisor) == 0){
                    // Si esta conectado se enviara el ack
                    if (usuarios[n].isConnected == true){
                        // Se crea el socket para enviar el ack al emisor
                        int socket_emisor = socket(AF_INET, SOCK_STREAM, 0);
                        struct sockaddr_in server_addr_em;
                        server_addr_em.sin_family = AF_INET;
                        server_addr_em.sin_port = htons(atoi(usuarios[n].puerto)); 
                        server_addr_em.sin_addr.s_addr = INADDR_ANY;
                        if (connect(socket_emisor, (struct sockaddr *)&server_addr_em, sizeof(server_addr_em)) == -1) {
                        printf("Error en la conexión\n");
                        close(socket_emisor);
                        }
                        // Se envia la operacion del ack
                        if (send(socket_emisor, &op2, sizeof(op2), 0) == -1) {
                        printf("Error en el envío\n");
                        close(socket_emisor);
                        }
                        // Se envia la id del mensaje
                        if (send(socket_emisor, &id_mensaje, sizeof(id_mensaje), 0) == -1) {
                        printf("Error en el envío\n");
                        close(socket_emisor);
                        }
                    }
                }
            }
            close(new_socket_c);
            //sleep(1);
        }
        // Se actualiza la lista de mensajes
        usuarios[pos_user].n_mensajes = 0;
        free(usuarios[pos_user].mensajes);
        usuarios[pos_user].mensajes = NULL;
    }
    
    pthread_mutex_unlock(&usuariosMutex);

}

void desconectar (void *user){
    // Funcion que desconecta al usuario
    struct usuario usuario_desconectar;
    pthread_mutex_lock(&mutex_mensaje);
    usuario_desconectar = (*(struct usuario *)user);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    char resultado = '3';
    pthread_mutex_lock(&usuariosMutex);
    int encontrado = 0;
    // Se busca al usuario que se quiere desonectar
    for (int i = 0; i < n_usuarios; i++){
        if (strcmp(usuario_desconectar.alias, usuarios[i].alias) == 0) {
            // Si el usuario esta conectado se desconecta y se resetea su puerto
            if (usuarios[i].isConnected == true){
                usuarios[i].isConnected = false;
                strcpy (usuarios[i].puerto , "-1");
                strcpy(usuarios[i].ip, "-1");
                resultado = '0';
                printf("DISCONNECT %s OK\n", usuario_desconectar.alias);}
            // Si el usuario no esta conectado salta un error
            else {
                resultado = '2';
                printf("DISCONNECT %s FAIL\n", usuario_desconectar.alias);
            }
            encontrado = 1;
        }
    }
    // Si no existe el usuario error
    if (encontrado == 0){
        resultado = '1';
        printf("DISCONNECT %s FAIL\n", usuario_desconectar.alias);
    }
    // Se envia el resultado de la funcion
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    pthread_mutex_unlock(&usuariosMutex);

}

void enviar_mensaje (void *param){
    // Funcion que envia un mensaje a otro usuario
    struct param_send parametros;
    pthread_mutex_lock(&mutex_mensaje);
    parametros = (*(struct param_send *)param);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    char resultado = '2';
    pthread_mutex_lock(&usuariosMutex);
    int existe_dest = 0;
    int existe_user = 0;
    int pos_dest;
    int pos_or;
    char id[256];
    memset(id , '\0' , sizeof(id));
    int puerto;
    int puerto_emisor;
    bool isconnected_emisor, isconnected_receptor;
    // Se busca al destinatario y al emisor y se almacenan sus puertos y su estado de conectado ademas de su posicion
    for (int i = 0; i < n_usuarios; i++){
        if (strcmp(parametros.emisor, usuarios[i].alias) == 0) {
            existe_user = 1;
            isconnected_emisor = usuarios[i].isConnected;
            puerto_emisor = atoi(usuarios[i].puerto);
            pos_or = i;
        }
        if (strcmp(parametros.receptor, usuarios[i].alias) == 0){
            pos_dest = i;
            existe_dest = 1;
            puerto = atoi(usuarios[i].puerto);
            isconnected_receptor = usuarios[i].isConnected;
        }
    }
    sprintf(id ,"%d", contador_id);
    contador_id++; // Se añade uno al contador para cambiar de id
    if ((existe_dest != 1) || (existe_user != 1)){
        resultado = '1'; // Si alguno no existe error
    }
    else if(isconnected_emisor == false){
        resultado = '2'; // Si el emisor esta desconectado error
    }
    else {
        if (isconnected_receptor == false){ // Si el destinatario esta desconectado se guardan lso mensajes hasta que se conecte
            usuarios[pos_dest].n_mensajes += 1;
            if (usuarios[pos_dest].n_mensajes == 1) { // Sino hay mensajes malloc
                usuarios[pos_dest].mensajes = malloc(sizeof(struct mensaje));
            } else { // Si ya hay mensajes se modiifica el tamano de las lista de mensajes
                usuarios[pos_dest].mensajes = realloc(usuarios[pos_dest].mensajes, (usuarios[pos_dest].n_mensajes) * sizeof(struct mensaje));
            }
            // Añadimos el mensaje a la lista de usuarios
            struct mensaje msg_actual;
            strcpy (msg_actual.msg , parametros.mensaje_enviado);
            strcpy (msg_actual.emisor , parametros.emisor);
            strcpy (msg_actual.id , id);
            usuarios[pos_dest].mensajes[usuarios[pos_dest].n_mensajes - 1] = msg_actual;
            printf("MESSAGE %s FROM %s TO %s STORED\n", msg_actual.id, msg_actual.emisor, parametros.receptor);
            
        }
        resultado = '0';
    }
    // Se envia el resultado de la operacion 
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    // Si la operacion se ha hecho correctamente
    if (resultado == '0') {
        char id_mensaje[256];
        memset(id_mensaje, '\0', sizeof(id_mensaje));
        strcpy(id_mensaje, id);
        // Se envia la id del mensaje
        if (send(sc, &id_mensaje, sizeof(id_mensaje), 0) == -1){
        printf("Error en envio\n");
        close(sc);
        }
        // Si esta conectado el destinatario se envia el mensaje
        if (usuarios[pos_dest].isConnected == true){
            // Se crea el socket con el puerto del destinatario
            int new_socket = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(puerto); 
            server_addr.sin_addr.s_addr = INADDR_ANY;
            char op1[256];
            char op2[256];
            char emisor[256];
            // Se crean los dos string para las dos operaciones posibles
            memset(emisor, '\0', sizeof(emisor));
            strcpy(emisor, parametros.emisor);
            memset(op1, '\0', sizeof(op1));
            strcpy(op1, "SEND_MESSAGE");
            memset(op2, '\0', sizeof(op2));
            strcpy(op2, "SEND_MESS_ACK");
            // Se conecta el socket
            if (connect(new_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                printf("Error en la conexión\n");
                close(new_socket);
            }
            // Se envia la operacion , el emisor , la id y el mensaje
            if (send(new_socket, &op1, sizeof(op1), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket);
            }
            if (send(new_socket, &emisor, sizeof(emisor), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket);
            }
            if (send(new_socket, &id, sizeof(id), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket);
            }
            char msg_enviado[256];
            memset(msg_enviado, '\0', sizeof(msg_enviado));
            strcpy(msg_enviado , parametros.mensaje_enviado);
            
            if (send(new_socket, &msg_enviado, sizeof(msg_enviado), 0) == -1) {
                printf("Error en el envío\n");
                close(new_socket);
            }
            printf("SEND MESSAGE %s FROM %s TO %s\n", id, emisor, usuarios[pos_dest].alias);

            if (usuarios[pos_or].isConnected == true){
                // Si cuando se envia el emisor esta conectado recibira un ack
                int socket_emisor = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in server_addr_em;
                server_addr_em.sin_family = AF_INET;
                server_addr_em.sin_port = htons(puerto_emisor); 
                server_addr_em.sin_addr.s_addr = INADDR_ANY;
                if (connect(socket_emisor, (struct sockaddr *)&server_addr_em, sizeof(server_addr_em)) == -1) {
                printf("Error en la conexión\n");
                close(socket_emisor);
            }   
            // Se envia la operacion indicando que es un ack y su id
                if (send(socket_emisor, &op2, sizeof(op2), 0) == -1) {
                printf("Error en el envío\n");
                close(socket_emisor);
                }
                if (send(socket_emisor, &id, sizeof(id), 0) == -1) {
                printf("Error en el envío\n");
                close(socket_emisor);
                }
            }
            
        }
    }
    pthread_mutex_unlock(&usuariosMutex);

}
void usuariosconectados(void* user){
    // Funcion que envia los usuarios que hay conectados
    struct usuario usuario_connected_users;
    pthread_mutex_lock(&mutex_mensaje);
    usuario_connected_users = (*(struct usuario *)user);
    mensaje_no_copiado = false;
    pthread_cond_signal(&cond_mensaje);
    pthread_mutex_unlock(&mutex_mensaje);
    pthread_mutex_lock(&usuariosMutex);
    char resultado = '0';
    int n_usuarios_conectados = 0;
    char cadena_n_usuarios[256];
    int encontrado = 0;
    // Se busca al usuario , si no esta conectado error
    for (int n = 0; n < n_usuarios; n++){
        if (strcmp(usuarios[n].alias, usuario_connected_users.alias) == 0){
            encontrado = 1;
            if (usuarios[n].isConnected == false){
                resultado = '1';
                printf("CONNECTEDUSERS FAIL\n");
            }
        }
    }
    // Si no se encuentra al usuario que ha ejecutado la funcion error 2
    if (encontrado == 0){
        resultado = '2';
        printf("CONNECTEDUSERS FAIL\n");
    }
    // Se envia el resultado de la funcion
    if (send(sc, &resultado, sizeof(char), 0) == -1){
        printf("Error en envio\n");
        close(sc);
    }
    // Si ha salido todo bien enviara los usuarios conectados
    if (resultado == '0'){
        printf("CONNECTEDUSERS OK\n");
        // Se hace un contador para ver cuantos usuarios estan conectados
        for (int i = 0; i < n_usuarios; i++){
            if (usuarios[i].isConnected == true){
                n_usuarios_conectados++;
            }
        }
        
        memset(cadena_n_usuarios , '\0' , sizeof(cadena_n_usuarios));
        sprintf(cadena_n_usuarios, "%d", n_usuarios_conectados);
        // Se envia el numero de usuarios conectados
        if (send(sc, &cadena_n_usuarios, sizeof(cadena_n_usuarios), 0) == -1){
            printf("Error en envio\n");
            close(sc);
        }
        // Se van enviando los usuarios que estan conecyados
        for (int j = 0; j < n_usuarios; j++){
            if (usuarios[j].isConnected == true){
                char alias_actual[256];
                memset(alias_actual, '\0', sizeof(alias_actual));
                strcpy(alias_actual, usuarios[j].alias);
                if (send(sc, &alias_actual, sizeof(alias_actual), 0) == -1){
                    printf("Error en envio\n");
                    close(sc);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&usuariosMutex);
}
int main(int argc , char* argv[]) {
    // Si no hay dos parametros falla
    if (argc != 2){
        printf("Parametros incorrectos\n");
        return -1;
    }
    pthread_attr_t t_attr; // atributos de los threads
    pthread_t thid;

    struct sockaddr_in server_addr,  client_addr;
    socklen_t size;
    // Se declara el socket
    if ((sd =  socket(AF_INET, SOCK_STREAM, 0))<0){
        printf ("SERVER: Error en el socket");
        return (0);
    }

    // Se establece la configuracion del socket
    val = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    // Inicializa la direccion del servidor a 0 para borrrar posibles datos basura
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(atoi(argv[1]));

    // Se configura la direccion del servidor
    err = bind(sd, (const struct sockaddr *)&server_addr,sizeof(server_addr));
    if (err == -1) {
        printf("Error en bind\n");
        return -1;
    }

    // Se escucha al socket
    err = listen(sd, SOMAXCONN);
    if (err == -1) {
        printf("Error en listen\n");
        return -1;
    }

    size = sizeof(client_addr);

    // Se inicializan los mutex y variables conducion
    pthread_mutex_init(&mutex_mensaje, NULL);
    pthread_cond_init(&cond_mensaje, NULL);
    pthread_attr_init(&t_attr);

    // atributos de los threads, threads independientes
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    while(1) {
        printf("esperando conexion\n");

        // Esperamos a la conexion
        sc = accept(sd, (struct sockaddr *) &client_addr, (socklen_t *) &size);
        if (sc == -1) {
            printf("Error en accept\n");
            return -1;
        }
        // Cuando tenemos conexion imprimimos el puerto y la IP
        printf("conexión aceptada de IP: %s   Puerto: %d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        // Se recibe la operacion
        char operacion[256];
        err = recv(sc, &operacion, sizeof(operacion), 0);   // recibe la operación
        if (err == -1) {
            printf("Error en recepcion\n");
            close(sc);
            continue;
        }
        // Dependiendo de que operacion sea se reciben unos parametros o otros
        if (strcmp("REGISTER" , operacion) == 0){
                struct usuario user;
                // Se reciben los parametros que necesita registrar
                err = recv(sc, &user.username, sizeof(user.username), 0);   
                if (err == -1) {
                    printf("Error en recepcion\n");
                    close(sc);
                    continue;
                }

                err = recv(sc, &user.alias, sizeof(user.alias), 0);   
                if (err == -1) {
                    printf("Error en recepcion\n");
                    close(sc);
                    continue;
                }
                err = recv(sc, &user.date, sizeof(user.date), 0);   
                if (err == -1) {
                    printf("Error en recepcion\n");
                    close(sc);
                    continue;
                }

                // Se crea el hilo y se envia la funcion
                if (pthread_create(&thid, &t_attr, (void *) registrar, (void *)&user) == 0) {
                    // se espera a que el thread copie el mensaje
                    pthread_mutex_lock(&mutex_mensaje);
                    while (mensaje_no_copiado)
                        pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                    mensaje_no_copiado = true;
                    pthread_mutex_unlock(&mutex_mensaje);
                }
                pthread_join(thid, NULL);
        }
        else if (strcmp("UNREGISTER" , operacion) == 0){
            struct usuario user;
            // Se reciben los parametros que necesita desregistrar
            err = recv(sc, &user.alias, sizeof(user.alias), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }

            // Se crea el hilo y se envia la funcion
            if (pthread_create(&thid, &t_attr, (void *) deregistrar, (void *)&user) == 0) {
                // se espera a que el thread copie el mensaje
                pthread_mutex_lock(&mutex_mensaje);
                while (mensaje_no_copiado)
                    pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                mensaje_no_copiado = true;
                pthread_mutex_unlock(&mutex_mensaje);
            }
            pthread_join(thid, NULL);
        } 
        else if (strcmp("CONNECT" , operacion) == 0){
            struct usuario user;
            strcpy(user.ip, inet_ntoa(client_addr.sin_addr));
            // Se reciben los parametros que necesita connect
            err = recv(sc, &user.alias, sizeof(user.alias), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            err = recv(sc, &user.puerto, sizeof(user.puerto), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            // Se crea el hilo y se envia la funcion
            if (pthread_create(&thid, &t_attr, (void *) conectar, (void *)&user) == 0) {
                // se espera a que el thread copie el mensaje
                pthread_mutex_lock(&mutex_mensaje);
                while (mensaje_no_copiado)
                    pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                mensaje_no_copiado = true;
                pthread_mutex_unlock(&mutex_mensaje);
            }
            pthread_join(thid, NULL);

        }
        else if (strcmp("DISCONNECT" , operacion) == 0){
            struct usuario user;
            // Se reciben los parametros que necesita disconnect
            err = recv(sc, &user.alias, sizeof(user.alias), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            // Se crea el hilo y se envia la funcion
            if (pthread_create(&thid, &t_attr, (void *) desconectar, (void *)&user) == 0) {
                // se espera a que el thread copie el mensaje
                pthread_mutex_lock(&mutex_mensaje);
                while (mensaje_no_copiado)
                    pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                mensaje_no_copiado = true;
                pthread_mutex_unlock(&mutex_mensaje);
            }
            pthread_join(thid, NULL);
        }
        else if (strcmp("SEND" , operacion) == 0){
            struct param_send parametros;
            // Se reciben los parametros que necesita send
            err = recv(sc, &parametros.emisor, sizeof(parametros.emisor), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            err = recv(sc, &parametros.receptor, sizeof(parametros.receptor), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            err = recv(sc, &parametros.mensaje_enviado, sizeof(parametros.mensaje_enviado), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }
            // Se crea el hilo y se envia la funcion
            if (pthread_create(&thid, &t_attr, (void *) enviar_mensaje, (void *)&parametros) == 0) {
                // se espera a que el thread copie el mensaje
                pthread_mutex_lock(&mutex_mensaje);
                while (mensaje_no_copiado)
                    pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                mensaje_no_copiado = true;
                pthread_mutex_unlock(&mutex_mensaje);
            }
            pthread_join(thid, NULL);
        }
        else if (strcmp("CONNECTEDUSERS" , operacion) == 0){
            // Se reciben los parametros que necesita usuarios conectados
            struct usuario user;
            err = recv(sc, &user.alias, sizeof(user.alias), 0);   
            if (err == -1) {
                printf("Error en recepcion\n");
                close(sc);
                continue;
            }

            // Se crea el hilo y se envia la funcion
            if (pthread_create(&thid, &t_attr, (void *) usuariosconectados, (void *)&user) == 0) {
                // se espera a que el thread copie el mensaje
                pthread_mutex_lock(&mutex_mensaje);
                while (mensaje_no_copiado)
                    pthread_cond_wait(&cond_mensaje, &mutex_mensaje);
                mensaje_no_copiado = true;
                pthread_mutex_unlock(&mutex_mensaje);
            }
            pthread_join(thid, NULL);;
        }
        else {
            printf("Operacion no valida\n");
            exit(-1);
        }
        }

        
        
    
    close (sd);
    return 0;
}