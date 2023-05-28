import subprocess
import sys
import PySimpleGUI as sg
from enum import Enum
import argparse
import socket
import time
import threading
from suds.client import Client
isConnected = False
event = threading.Event()

def poner_padding(variable):
    # Funcion que anade el paddign a la variable pasada por parametro 
    for _ in range(256 - len(variable)): # Siempre anadira hasta 256 caracteres
        variable += "\0"
    return variable

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _quit = 0
    _username = None
    _alias = None
    _date = None
    _socket = 0
    # ******************** METHODS *******************
    # *
    # * @param user - User name to register in the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user, window):
        # Funcion para registrar usuarios
        # Se crea el socket
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        operacion_register = "REGISTER" # Operacion a realizar
        # Se conecta el socket y se envian los datos
        client._socket.connect((client._server, client._port))
        client._socket.send(poner_padding(operacion_register).encode())
        client._socket.send(poner_padding((client._username)).encode())
        client._socket.send(poner_padding((client._alias)).encode())
        client._socket.send(poner_padding((client._date)).encode())
        # Se recibe la respuesta
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        # Se decodifica la respuesta
        if respuesta == "0":
            window['_SERVER_'].print("s> REGISTER OK")
        elif respuesta == "1":
            window['_SERVER_'].print("s> USERNAME IN USE")
            client._alias = None
        else:
            window['_SERVER_'].print("s> REGISTER FAIL")
            client._alias = None
        client._socket.close()
        return client.RC.ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 *
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user, window):
        # Se crea el socket
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket.connect((client._server, client._port))
        operacion_unregister = "UNREGISTER" # Operacion de la funcion
        client._socket.send(poner_padding(operacion_unregister).encode())
        client._socket.send(poner_padding(client._alias).encode())
        # Se recibe la respuesta y se analiza el resultado
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        if respuesta == "0":
            window['_SERVER_'].print("s> UNREGISTER OK")
            client._alias = None
        elif respuesta == "1":
            window['_SERVER_'].print("s> USER DOES NOT EXIST")
        else: 
            window['_SERVER_'].print("s> UNREGISTER FAIL")
        client._socket.close()
        return client.RC.ERROR


    # *
    # * @param user - User name to connect to the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred

    def recibir_mensaje(sct , window):
        # Se lee la variable global que indica si esta conectado o no
        global isConnected 
        # Se añade un timeout para que pruebe a conectar durante dos segundos y si no repita el bucle
        sct.settimeout(2)
        sct.listen()
        while isConnected is True:
            # Bucle que se ejecuta hasta que el usuario se ha desconectado
            try:
                conn, addr = sct.accept()
                # Se recibe la operacion y se analiza
                op = conn.recv(256)
                op = op.decode().strip('\x00')
                if op == "SEND_MESSAGE":
                    # Si se recibe un mensaje se recibe el mensaje, la id y el emisor
                    aliasrem = conn.recv(256)
                    aliasrem = aliasrem.decode().strip('\x00')
                    idmensaje = conn.recv(256)
                    idmensaje = idmensaje.decode().strip('\x00')
                    mensajeuser = conn.recv(256)
                    mensajeuser = mensajeuser.decode().strip('\x00')
                    window['_SERVER_'].print("s> MESSAGE " + idmensaje + " FROM " + aliasrem + "\n" + mensajeuser + "\n END")
                elif op == "SEND_MESS_ACK":
                    # Si es una confirmacion de que el usuario ha recibido un mensaje se recibe la id
                    idmensaje = conn.recv(256)
                    idmensaje = idmensaje.decode().strip('\x00')
                    window['_SERVER_'].print("s> SEND MESSAGE " + idmensaje + " OK")
                conn.close()
            except TimeoutError:
                pass
        

    @staticmethod
    def conectar(user, window):
        socket_user = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ip = socket_user.getsockname()[0]
        socket_user.bind((ip, 0))
        puerto = socket_user.getsockname()[1]
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket.connect((client._server, client._port))
        operacion_connect = "CONNECT"
        client._socket.send(poner_padding(operacion_connect).encode())
        client._socket.send(poner_padding(client._alias).encode())
        client._socket.send(poner_padding(str(puerto)).encode())
        # Se recibe la respuesta y se analiza el resultado
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        if respuesta == "0":
            window['_SERVER_'].print("s> CONNECT OK")
            # Se actualiza la variable global para que se conecte
            global isConnected
            isConnected = True
            event.set()
            # Se crea un hilo que realiza la funcion recibir
            hilo = threading.Thread(target=client.recibir_mensaje, args=(socket_user, window, ))
            hilo.start()
        elif respuesta == "1":
            window['_SERVER_'].print("s> CONNECT FAIL, USER DOES NOT EXIST")
        elif respuesta == "2":
            window['_SERVER_'].print("s> USER ALREADY CONNECTED")
        else: 
            window['_SERVER_'].print("s> CONNECT FAIL")
        client._socket.close()
        return client.RC.ERROR


    # *
    # * @param user - User name to disconnect from the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred

    @staticmethod
    def  disconnect(user, window):
        # Se crea el soket
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket.connect((client._server, client._port))
        operacion_disconnect = "DISCONNECT" # Operacion 
        client._socket.send(poner_padding(operacion_disconnect).encode())
        client._socket.send(poner_padding(client._alias).encode())
        # Se recibe la respuesta y se analiza el resultado
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        if respuesta == "0":
            window['_SERVER_'].print("s> DISCONNECT OK")
            # Se actualiza la variable global para que deje de esperar mensajes
            global isConnected
            isConnected = False
            event.set()
        elif respuesta == "1":
            window['_SERVER_'].print("s> DISCONNECT FAIL / USER DOES NOT EXIST")
        elif respuesta == "2":
            window['_SERVER_'].print("s> DISCONNECT FAIL / USER NOT CONNECTED")
        else: 
            window['_SERVER_'].print("s> DISCONNECT FAIL")
        client._socket.close()
        return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user, message, window):
        # Se crea el socket
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket.connect((client._server, client._port))
        message = client.formatear_mensaje(message)
        operacion_send = "SEND" # Operacion send
        # Se envian los datos
        client._socket.send(poner_padding(operacion_send).encode())
        client._socket.send(poner_padding(client._alias).encode())
        client._socket.send(poner_padding(user).encode())
        client._socket.send(poner_padding(message).encode())
        print("SEND " + user + " " + message)
        # Se recibe la respuesta y se analiza el resultado
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        if respuesta == '0':
            # Si es correcto se recibe tambien la id
            id_bytes = client._socket.recv(256)
            id = id_bytes.decode().strip('\x00')
            window['_SERVER_'].print("s> SEND OK - MESSAGE " + str(id))
        elif respuesta == '1':
            window['_SERVER_'].print("s> SEND FAIL / USER DOES NOT EXIST")
        else: 
            window['_SERVER_'].print("s> SEND FAIL")
        client._socket.close()
        return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * @param file    - file  to be sent

    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user, message, file, window):
        window['_SERVER_'].print("s> SENDATTACH MESSAGE OK")
        print("SEND ATTACH " + user + " " + message + " " + file)
        #  Write your code here
        return client.RC.ERROR

    @staticmethod
    def connectedUsers(user, window):
        # Se crea el socket
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket.connect((client._server, client._port))
        operacion_connected_users = "CONNECTEDUSERS" # Operacion 
        # Se envian los datos
        client._socket.send(poner_padding(operacion_connected_users).encode())
        client._socket.send(poner_padding(client._alias).encode())
        # Se recibe la respuesta y se analiza el resultado
        respuesta_bytes = client._socket.recv(1)
        respuesta = respuesta_bytes.decode()
        if respuesta == '0':
            # Si la respuesta es afirmativa recibira todos los usuarios
            users = []
            num_users = client._socket.recv(256)
            num_users = int(num_users.decode('utf-8').strip('\x00'))
            for n in range(num_users):
                user = client._socket.recv(256)
                user = user.decode('utf-8').strip('\x00')
                users.append(user)
            window['_SERVER_'].print("s> CONNECTED USERS ({} users connected) OK - {}".format(num_users, ", ".join(users)))
        elif respuesta == '1':
            window['_SERVER_'].print("s> CONNECTED USERS FAIL / USER IS NOT CONNECTED")
        else:
            window['_SERVER_'].print("s> CONNECTED USERS FAIL")
        #  Write your code here
        return client.RC.ERROR


    @staticmethod
    def window_register():
        layout_register = [[sg.Text('Ful Name:'),sg.Input('Text',key='_REGISTERNAME_', do_not_clear=True, expand_x=True)],
                            [sg.Text('Alias:'),sg.Input('Text',key='_REGISTERALIAS_', do_not_clear=True, expand_x=True)],
                            [sg.Text('Date of birth:'),sg.Input('',key='_REGISTERDATE_', do_not_clear=True, expand_x=True, disabled=True, use_readonly_for_disable=False),
                            sg.CalendarButton("Select Date",close_when_date_chosen=True, target="_REGISTERDATE_", format='%d-%m-%Y',size=(10,1))],
                            [sg.Button('SUBMIT', button_color=('white', 'blue'))]
                            ]

        layout = [[sg.Column(layout_register, element_justification='center', expand_x=True, expand_y=True)]]

        window = sg.Window("REGISTER USER", layout, modal=True)
        choice = None

        while True:
            event, values = window.read()

            if (event in (sg.WINDOW_CLOSED, "-ESCAPE-")):
                break

            if event == "SUBMIT":
                if(values['_REGISTERNAME_'] == 'Text' or values['_REGISTERNAME_'] == '' or values['_REGISTERALIAS_'] == 'Text' or values['_REGISTERALIAS_'] == '' or values['_REGISTERDATE_'] == ''):
                    sg.Popup('Registration error', title='Please fill in the fields to register.', button_type=5, auto_close=True, auto_close_duration=1)
                    continue

                client._username = values['_REGISTERNAME_']
                client._alias = values['_REGISTERALIAS_']
                client._date = values['_REGISTERDATE_']
                break
        window.Close()
        
    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;

        client._server = args.s
        client._port = args.p

        return True
    
    def formatear_mensaje(mensaje):
        #Se conecta al servidor
        url = "http://localhost:5000/?wsdl"
        soap = Client(url) 
        result = soap.service.formatear_mensajes(mensaje)
        return result

    def main(argv):

        if (not client.parseArguments(argv)):
            client.usage()
            exit()
        
        lay_col = [[sg.Button('REGISTER',expand_x=True, expand_y=True),
                sg.Button('UNREGISTER',expand_x=True, expand_y=True),
                sg.Button('CONNECT',expand_x=True, expand_y=True),
                sg.Button('DISCONNECT',expand_x=True, expand_y=True),
                sg.Button('CONNECTED USERS',expand_x=True, expand_y=True)],
                [sg.Text('Dest:'),sg.Input('User',key='_INDEST_', do_not_clear=True, expand_x=True),
                sg.Text('Message:'),sg.Input('Text',key='_IN_', do_not_clear=True, expand_x=True),
                sg.Button('SEND',expand_x=True, expand_y=False)],
                [sg.Text('Attached File:'), sg.In(key='_FILE_', do_not_clear=True, expand_x=True), sg.FileBrowse(),
                sg.Button('SENDATTACH',expand_x=True, expand_y=False)],
                [sg.Multiline(key='_CLIENT_', disabled=True, autoscroll=True, size=(60,15), expand_x=True, expand_y=True),
                sg.Multiline(key='_SERVER_', disabled=True, autoscroll=True, size=(60,15), expand_x=True, expand_y=True)],
                [sg.Button('QUIT', button_color=('white', 'red'))]
            ]


        layout = [[sg.Column(lay_col, element_justification='center', expand_x=True, expand_y=True)]]

        window = sg.Window('Messenger', layout, resizable=True, finalize=True, size=(1000,400))
        window.bind("<Escape>", "-ESCAPE-")


        while True:
            event, values = window.Read()

            if (event in (None, 'QUIT')) or (event in (sg.WINDOW_CLOSED, "-ESCAPE-")):
                sg.Popup('Closing Client APP', title='Closing', button_type=5, auto_close=True, auto_close_duration=1)
                break

            #if (values['_IN_'] == '') and (event != 'REGISTER' and event != 'CONNECTED USERS'):
             #   window['_CLIENT_'].print("c> No text inserted")
             #   continue

            if (client._alias == None or client._username == None or client._alias == 'Text' or client._username == 'Text' or client._date == None) and (event != 'REGISTER'):
                sg.Popup('NOT REGISTERED', title='ERROR', button_type=5, auto_close=True, auto_close_duration=1)
                continue

            if (event == 'REGISTER'):
                client.window_register()

                if (client._alias == None or client._username == None or client._alias == 'Text' or client._username == 'Text' or client._date == None):
                    sg.Popup('NOT REGISTERED', title='ERROR', button_type=5, auto_close=True, auto_close_duration=1)
                    continue

                window['_CLIENT_'].print('c> REGISTER ' + client._alias)
                client.register(client._alias, window)

            elif (event == 'UNREGISTER'):
                window['_CLIENT_'].print('c> UNREGISTER ' + client._alias)
                client.unregister(client._alias, window)


            elif (event == 'CONNECT'):
                window['_CLIENT_'].print('c> CONNECT ' + client._alias)
                client.conectar(client._alias, window)


            elif (event == 'DISCONNECT'):
                window['_CLIENT_'].print('c> DISCONNECT ' + client._alias)
                client.disconnect(client._alias, window)


            elif (event == 'SEND'):
                window['_CLIENT_'].print('c> SEND ' + values['_INDEST_'] + " " + values['_IN_'])

                if (values['_INDEST_'] != '' and values['_IN_'] != '' and values['_INDEST_'] != 'User' and values['_IN_'] != 'Text') :
                    client.send(values['_INDEST_'], values['_IN_'], window)
                else :
                    window['_CLIENT_'].print("Syntax error. Insert <destUser> <message>")


            elif (event == 'SENDATTACH'):

                window['_CLIENT_'].print('c> SENDATTACH ' + values['_INDEST_'] + " " + values['_IN_'] + " " + values['_FILE_'])

                if (values['_INDEST_'] != '' and values['_IN_'] != '' and values['_FILE_'] != '') :
                    client.sendAttach(values['_INDEST_'], values['_IN_'], values['_FILE_'], window)
                else :
                    window['_CLIENT_'].print("Syntax error. Insert <destUser> <message> <attachedFile>")


            elif (event == 'CONNECTED USERS'):
                window['_CLIENT_'].print("c> CONNECTEDUSERS")
                client.connectedUsers(client._alias, window)



            window.Refresh()

        window.Close()

if __name__ == '__main__':
    client.main([])
    print("+++ FINISHED +++")

