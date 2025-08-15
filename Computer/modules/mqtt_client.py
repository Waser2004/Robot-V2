import socket

class MQTTClient():
    """MQTT Client for connecting to a server using mDNS resolution."""
    # Server configuration
    _host_name = "RobotV2.local"
    _port = 80

    def __init__(self, host_name=None, port=None):
        self._host_name = host_name if host_name else self._host_name
        self._port = port if port else self._port

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.subs = {}

    async def connect(self):
        print(f"Connecting to {self._host_name} on port {self._port}...")
        # reslove mDNS
        try: 
            self.ip_address = socket.gethostbyname(self._host_name)
        except socket.gaierror:
            print(f"Failed to resolve mDNS hostname: {self._host_name}")
            return None

        # cinnect to the server
        self.sock.connect((self.ip_address, self._port))
        print(f"Connected to {self._host_name} at {self.ip_address}:{self._port}")
    
    def close(self):
        self.sock.close()
    
    # sub / pub methods
    async def pub(self, topic, message):
        """Publish a message to a topic."""
        msg = f'pub {topic} {message}\n'
        byte_msg = msg.encode('utf-8')
        print(f"Sending {len(byte_msg)} bytes: {msg!r}")
        self.sock.sendall(msg.encode('utf-8'))

    async def sub(self, topic, callback):
        """Subscribe to a topic."""
        if topic not in self.subs:
            self.subs.update({topic: [callback]})
        else:
            self.subs[topic].append(callback)

        print(self.subs)
        
    # handle incoming message
    async def loop(self):
        """Handle incoming messages."""
        buffer = ''
        while True:
            # read data from the socket
            data = self.sock.recv(256)
            if not data:
                break
            
            # decode and process the data
            buffer += data.decode('utf-8')
            while '\n' in buffer:
                # extract one message at a time
                line, buffer = buffer.split('\n', 1)
                message = line.strip()
                if not message:
                    continue
                    
                # check format of the message
                print(f"Received: {message}")
                parts = message.split(' ', 2)
                if len(parts) < 3 or parts[0] != 'pub':
                    continue
            
                # extract topic and payload
                topic, payload = parts[1], parts[2]
            
                # call the callback for the topic
                if topic in self.subs:
                    for callback in self.subs[topic]:
                        await callback(topic, payload)
        