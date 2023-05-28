from spyne import Application, ServiceBase, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication
import time

# Clase para dar formato a los mensajes 
class Formato(ServiceBase):

    @rpc(Unicode, _returns=Unicode)
    def formatear_mensajes(ctx, mensaje):
        # Funcion que formatea los mensajes y quita los dobles espacios
        mensaje = ' '.join(mensaje.split())
        return mensaje # Se devuelve el mensaje formateado
    
    

# Creacion de la aplicacion 
aplicacion = Application(
    services=[Formato],
    tns='http://tests.python-zeep.org/',
    # Protocolos 
    in_protocol=Soap11(validator='lxml'),
    out_protocol=Soap11())

aplicacion = WsgiApplication(aplicacion)

if __name__ == '__main__':

    from wsgiref.simple_server import make_server
    server = make_server('0.0.0.0', 5000, aplicacion)
    # Se inicializa el servidor
    server.serve_forever()
