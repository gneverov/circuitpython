# MicroPythonRT demos
- [audio player](/examples/async/audio_player.md)
- [remote UART](/examples/async/remote_uart.md)

## Network setup
Both apps require a network connection to the device. Getting a network connection depends on which board you are using

### RPI PICO W
This board has a wifi adapter and uses the same code as MicroPython for configuring the wifi adapter. The only real difference is that the `WLAN` is in the `rp2` module since the `network_cyw43` does not exist.
```
import rp2
wifi = rp2.WLAN()
wifi.active(True)
wifi.connect("mywifi", "mypassword")
```
Once the above code succeeds at connecting to a wifi network, the network itself can be configured using the new `netif` object.
```
import socket
socket.netif['wl0'].dhcp_start()
print('Waiting for network...')
socket.netif['wl0'].wait()
socket.netif.default = socket.netif['wl0']
```
The `netif` object exposes much of lwIP's netif API which gives you a lot of flexibility on how the network is configured. The wifi adapter is assigned the interface name 'wl0'. Calling `dhcp_start` tells the interface to start configuring itself using DHCP. The `wait` method blocks until the interface gets an IP address. This is useful to prevent other code from running before the network is set up. No CPU is consumed while waiting and other tasks are free to run. Finally assigning the interface object to `netif.default` sets the interface as the default route.

To manually configure the network interface instead of using DHCP:
```
socket.netif['wl0'].configure('192.168.0.100', '192.168.0.1', '255.255.255.0')
```
Where the 3 IP addresses are the local address, gateway address, and netmask, in that order.

### RPI PICO
This board does not have a wifi adapter so it wouldn't usually have a network connection. However it is possible to use a USB connection for networking. This is the same technology that USB network adapters use. I have only tested this functionality on Windows.

In Windows, open the Network Connections control panel. (From command prompt or run dialog, run `ncpa.cpl`.) When you connect the board to USB, a new network adapter should appear with the name "Remote NDIS based Internet Sharing Device". From here the easiest way to connect the board to the Internet is to create a virtual Ethernet bridge between the RNDIS adapter and whatever adapter your Windows machine uses for Internet. In Network Connections control panel, select the RNDIS adapter and the other adapter, right click and select "Create Bridge". Hopefully that works, but its not a very robust procedure and many things can go wrong.

Once you have the network set up on the Windows side, you can configure the network on the board. This is pretty much the same as in the wifi case except that the interface name is 'sl0' instead of 'wl0'.
```
import socket
socket.netif['sl0'].dhcp_start()
print('Waiting for network...')
socket.netif['sl0'].wait()
socket.netif.default = socket.netif['sl0']
```
USB networking is also supported on RPI PICO W in addition to its wifi.

## Network troubleshooting
Printing the net interface object gives you basic info about the interface: its IP address and link status.
```
>>> socket.netif['sl0']
NetInterface(name=sl0, address=192.168.0.100, link=up)
```
The board should respond to pings if its network is working, so you can ping it from a computer.

To see the DNS servers:
```
>>> socket.dns_servers
['8.8.8.8']
```
