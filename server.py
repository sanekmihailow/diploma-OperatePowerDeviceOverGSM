    # This file is part of program.
    # 
    #                        ***Important***
    # This file contains instructions for sending control signals from Onion Omega2 
    # (which is connected to the uart with the GSM module)to Arduino.
    # 
    #Copyright (c) 2019 Mikhailov Alexandr <sanekmihailow@gmail.com>
    #This program is free software: you can redistribute it and/or modify
    # it under the terms of the GNU General Public License version 2 as published by
    # the Free Software Foundation.
    # 
    # This program is distributed in the hope that it will be useful,
    # but WITHOUT ANY WARRANTY; without even the implied warranty of
    # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    # GNU General Public License for more details.
    # 
    # You should have received a copy of the GNU General Public License
    # along with program. If not, see <https://www.gnu.org/licenses/>.

import socket
import serial
import select
import time
import sys

SERVER_ADDRESS = ('0.0.0.0', 5005)
GSM_ADRESS = '/dev/ttyS1'

AT_CMGF = 'AT+CMGF'
AT_CMGL = 'AT+CMGL'
AT_CMGR = 'AT+CMGR'
AT_CMGD = 'AT+CMGD'

GSM_NEW_SMS = '+SMSFLAG'
GSM_TEXT_MODE = '1'
GSM_SMS_UNREAD = 'REC UNREAD'

CMD_POWER_ON = 'pon'
CMD_POWER_OFF = 'pof'
CMD_RESET = 'rst'

ADMIN_TEL = '+79203079273'

#1 секунда для ожидания запроса
gsm_serial = serial.Serial(GSM_ADRESS, timeout=1)
server_socket = None
slave_socket = None

slave_conn = None
slave_addr = None
slave_connected = False
slave_ready = False

def gsm_check_ok():

    for check_attemt in range(5):
        #ждем пока отобьется команда
        time.sleep(1)
        #можем читать максимального количества байт
        answer = gsm_serial.read(100)
        if answer.decode().find('OK') >= 0:
            return True
    return False


def gsm_sync():
    gsm_serial.write('AT\r\n')
    return gsm_check_ok()

def gsm_set_mode(mode):
    gsm_serial.write(AT_CMGF + '=' + mode +'\r\n')
    return gsm_check_ok()

def gsm_del(id):
    gsm_serial.write(bytes(AT_CMGD + '=' + id + '\r\n'))
    return gsm_check_ok()


def gsm_parse_sms(id):
    gsm_serial.write(bytes(AT_CMGR + '=' + id + '\r\n'))
    time.sleep(2)
    answer = gsm_serial.read(100)
    sms_str = answer.decode()
    if not gsm_del(id):
        print 'Warning: unable to delete sms'
    if sms_str.find(ADMIN_TEL) == -1: #если телефон не найден
        print 'Not from admin'
        return None
    if sms_str.find(CMD_POWER_ON) >= 0: #если find нашел команду от 0 и дальше - возвращает pon
        return CMD_POWER_ON
    elif sms_str.find(CMD_POWER_OFF) >= 0:
        return CMD_POWER_OFF
    elif sms_str.find(CMD_RESET) >= 0:
        return CMD_RESET
    else:
        print 'Unsupported command '
        return None


def gsm_get_unreads():
    gsm_serial.write(bytes(AT_CMGL + '="REC UNREAD"\r\n'))
    time.sleep(2)
    answer = gsm_serial.read(200)
    print 'Unreads:\'\n' + answer + '\n\''
    sms_idx = answer.find('\x2BCMGL\x3A') #+CMGL:
    sms_id = 0
    print 'Find result is ' + str(sms_idx)
    if sms_idx >= 0:
        sms_idx = sms_idx + len('\x2BCMGL\x3A') #прибавляем длину чтобы передвинуться в конец (к id)
        print 'Id is ' + answer[sms_idx + 1]
        return gsm_parse_sms(answer[sms_idx + 1])
    return None


def is_slave_connected():
    # разрешаем на редактировании
    global slave_conn
    global slave_addr
    try:
        slave_conn, slave_addr = server_socket.accept() #прием соединения
        print 'New connection from '+ slave_addr[0] + ':' + str(slave_addr[1]) #показываем адрес и порт соединившегося
        return True
    except socket.timeout:
        return False
    else:
        return False
    return False


def is_slave_ready():
    global slave_conn
    data = slave_conn.recv(1024) # ожидание не более 1024 байт
    if '+SLAVE0:READY' in data:
        return True
    return False

try:
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM) #tcp/ip  stream
    server_socket.settimeout(1)
    server_socket.bind(SERVER_ADDRESS)
    server_socket.listen(1)
    print 'Server socket initialized'

    if gsm_sync() == False:
        sys.exit("No GSM sync")

    print 'GSM synced'

    if not gsm_set_mode(GSM_TEXT_MODE):
        sys.exit("Unable to switch GSM to text mode")

    print 'GSM switched to text mode'

    while True:
        if not slave_connected:
            print 'Polling for slave connection'
            slave_connected = is_slave_connected()
            if not slave_connected:
                continue #закончить цикл и перейти в начало while
            else:
                print 'Slave connected'

        if not slave_ready:
            print 'Polling for ready signal'
            slave_ready = is_slave_ready()
            if not slave_ready:
                continue
            else:
                print 'Slave ready'

        server_cmd = gsm_get_unreads()
        if server_cmd == None:
            continue
        print 'server cmd - ' + server_cmd

        print 'Sending to ' + slave_addr[0]
        # коннект к ардуинке
        if slave_socket == None:
            slave_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            slave_socket.settimeout(1)
            slave_socket.connect((slave_addr[0], 5005))
        slave_socket.sendall(server_cmd + '\n')
except KeyboardInterrupt:
    server_socket.close()
    raise
