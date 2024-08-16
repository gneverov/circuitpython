# Network configuration
The `network` module provides a wrapper around the LwIP APIs for managing network interfaces and services. The `netif` attribute returns a list-like object of all the network interfaces registered with LwIP.
```
>>> import network
>>> list(network.netif)
[NetInterface(name=sl0, address=0.0.0.0, link=up)]
```
The network interface it is showing is the USB network interface. Because of reasons, this interface always shows up when TinyUSB is compiled with network support regardless of whether a USB network device descriptor is actually configured. But by default there is no network interface enabled.

## Enable network interface
First we need to enable some hardware on the board to provide a network interface. There are two possible options: USB and wifi.

The RPI PICO board does not have a wifi adapter, so it is only possible to use USB for networking. The RPI PICO W has a CYW43 wifi adapter, so it can use both wifi and USB. Other boards may have different types of wifi adapters, which should work, but currently not all of them have been ported from MicroPython.

### Enable USB network
TinyUSB provides 3 kinds of USB network devices: RNDIS, ECM, and NCM. Only RNDIS works for me on Windows, and I haven't tried anything on Linux. All 3 kinds can be enabled through the `UsbConfig` class. However TinyUSB only compiles with NCM only or both ECM and RNDIS support. It doesn't compile with all three. Overall I wouldn't say this support is very easy to use because of configuration issues on the host side.

To configure the board as a RNDIS network adapter, run:
```
import usb
cfg = usb.UsbConfig()
cfg.device()
cfg.configuration()
cfg.net_rndis()
cfg.cdc()
cfg.msc()
cfg.save()
```
(N.B. It seems important that the RNDIS descriptor is the first descriptor in the configuration, before other descriptors for CDC and MSC.)

In Windows, open the Network Connections control panel. (From command prompt or run dialog, run `ncpa.cpl`.) When you connect the board to USB, a new network adapter should appear with the name "Remote NDIS based Internet Sharing Device". From here the easiest way to connect the board to the Internet is to create a virtual Ethernet bridge between the RNDIS adapter and whatever adapter your Windows machine uses for Internet. In Network Connections control panel, select the RNDIS adapter and the other adapter, right click and select "Create Bridge". Hopefully that works, but its not a very robust procedure and many things can go wrong.

### Enable CYW43 wifi
Install the `cyw43` extension module following these [instructions](/getstarted.md#adding-extension-modules) for adding extension modules. Once installed, you will be able to import the `cyw43` module.
```
import cyw43
wifi = cyw43.WLAN()
wifi.active(True)
wifi.connect("mywifi", "mypassword")
```

A side-effect of importing `cyw43` is that a new network interface named "wl1" will appear in the `netif` list.
```
>>> list(network.netif)
[NetInterface(name=sl0, address=0.0.0.0, link=up), NetInterface(name=wl1, address=0.0.0.0, link=up)]
```

Optionally, to specify the country mode of the wifi radio using a 2-letter [country code](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2), run:
```
import os
os.putenv('COUNTRY', 'US')
```

## Configure network interface
Now that we have a network interface enabled, we can continue setting it up. First for convenience, grab a reference to the network interface you're using.
```
# Use 'wl1' for wifi or 'sl0' for USB
netif = network.netif['wl1']
```

### Configure a network interface using DHCP
```
# Start the DHCP client
netif.dhcp_start()

# Optionally wait for an IP address to be assigned
netif.wait() 

# Set the default route
network.netif.default = netif
```
Waiting for the DHCP process to complete is useful for preventing other code from running before the network is set up.

The last line to set the default route is easily forgetten but very important. It tells LwIP which interface to use to access the Internet. Without it you won't connect to the Internet.

### Configure a network interface manually
```
# Set the local address, netmask, and gateway
netif.configure('192.168.0.100', '255.255.255.0', '192.168.0.1')

# Set the DNS server
network.dns_servers(['8.8.8.8'])

# Set the default route
network.netif.default = netif
```

## Testing the network
Printing the net interface object gives you basic info about the interface such as its IP address and link status.
```
>>> network.netif['sl0']
NetInterface(name=sl0, address=192.168.0.100, link=up)
```

The board should respond to pings if its network is working, so you can ping it from a computer. It is also possible to ping from the board.
```
>>> network.ping('github.com')
Reply from 140.82.116.4: bytes=32 time=10ms TTL=52
...
```

To see the DNS servers:
```
>>> network.dns_servers()
['8.8.8.8']
```
