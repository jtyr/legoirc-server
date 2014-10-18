LEGO IR Controller Server
=========================


Description
-----------

This is the server side of the LEGO IR Controller system which is trying to
provide an alternative to the original IR controller by using a [smartphone
application](https://github.com/jtyr/legoirc-android). The smartphone application
communicates with the IR receiver on the vehicle (e.g. [9398 4x4
Crawler](http://www.lego.com/en-gb/technic/products/speed/9398-4x4-crawler))
through an IR transmitter controlled by a [Raspberry
Pi](http://www.raspberrypi.org) installed on the vehicle. The smartphone
communicates with the Raspberry Pi through Wifi signal which allows to extend the
operation range and makes the control of the vehicle less prone to signal loss
which is often experienced with the original IR controller. This solution also
allows to use the [Raspberry Pi
Camera](http://www.raspberrypi.org/products/camera-module/) to stream realtime
video from the vehicle and display it on the screen of the smartphone. The
installation is non-invasive and doesn't require any modification of the vehicle.

![9398 4x4 Crawler with Raspberry Pi](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/car.png)


Requirements
------------

To make this project working with your LEGO Technic vehicle, you should have the
following:

- [Raspberry PI B+](http://www.amazon.co.uk/Raspberry-Pi-Desktop-700MHz-Processor/dp/B00LPESRUK) (£25.70)
- [Raspberry PI case](http://www.amazon.co.uk/Pimoroni-Ninja-PiBow-Layer-Raspberry/dp/B00DUUBSVM) (£11.98)
- [Raspberry Pi camera](http://www.amazon.co.uk/Raspberry-Pi-100003-Camera-Module/dp/B00E1GGE40) (£19.25)
- [External battery](http://www.amazon.co.uk/Ultra-Compact-Lipstick-Sized-Portable-External-Technology/dp/B005QI1A8C) (£13.99)
- [IR tramsmitter](http://www.ebay.co.uk/itm/NEW-38KHz-Digital-IR-Infrared-Sensor-Transmitter-Kit-for-Arduino-Raspberry-Pi-/131076249674) (£5.25)
- [Wifi dongle](http://www.amazon.co.uk/CSL-connector-removable-connection-particularly/dp/B00DY5ZN3W) (£10.85)
- [Jumper Cable](http://www.amazon.co.uk/Rainbow-Color-Arduino-Jumper-Appliance/dp/B00IOB9IU0) (£2.09)

Total cost: £89.11


Installation
------------

The installation is described for the [9398 4x4
Crawler](http://www.lego.com/en-gb/technic/products/speed/9398-4x4-crawler)).
Similar setup can be applied with small modifications to any other vehicles.

The IR transmitter should be connected to the Raspberry Pi on the PIN 17 (`VCC`),
18 (`DAT`) and 19 (`GND`) as shown on the following image:

![IR Transmittor pinout](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/irtx_pinout.png)

The IR transmitter can be installed on the top of the roof facing towards the IR
receiver. Because even sunlight can interrupt the IR communication, it's recommended
to cover both the IR transmitter and the receiver.

![IR Transmittor installation](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/photo_irtx.png)

The Raspberry Pi itself fits nicely into the rear cargo area of the car. It only
requires to remove the site bars and install them in horizontal position above
the Raspberry Pi.

![Raspberry Pi installation](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/photo_rpi.png)

The battery for the Raspberry Pi should be installed above the front axle to
balance the weight.

![External battery installation](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/photo_battery.png)

Raspberry Pi Camera can be installed either on the bar above the IR receiver or
in the from of the car (requires longer [flex
cable](http://www.adafruit.com/products/1730)).

![Raspberry Pi Camera installation](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/photo_camera.png)


Raspberry PI configuration
--------------------------

The installation of the `legoirc-server` on the Raspberry Pi is described for
[Arch Linux ARM](http://archlinuxarm.org/platforms/armv6/raspberry-pi). For other
distributions it's necessary to compile it manually. In general, the
`legoirc-server` depends on [`bcm2835`
library](http://www.airspayce.com/mikem/bcm2835/index.html) which must be
installed before the compilation. If your distribution is not using
[`systemd`](http://en.wikipedia.org/wiki/Systemd) then you have to write your own
`init.d` scripts or run the `legoirc-server` manually every time you start up the
Raspberry Pi.

The installation procedure on the Arch Linux ARM, including the installation of
`systemd` services, is as follows:

```
pacman -S base-devel
curl -o PKGBUILD https://aur.archlinux.org/packages/le/legoirc-server/PKGBUILD
makepkg --asroot --install --syncdeps
```


Protocol
--------

Each command sent to the server should be terminated by a newline (`\n`). If the
command consists only of a newline, the connection will be closed. The commands
can be sent by a specially created [Android
application](https://github.com/jtyr/legoirc-android) or by the `legoirc-client`
which is part of this project:

```
legoirc-client -s <IP_of_your_RPi>
```

Even easier is to use `telnet`:

```
telnet <IP_of_your_RPi> 8160
```

The direction commands correspond with the layout of the keys on the numeric
keyboard:

![Commands](https://raw.githubusercontent.com/jtyr/legoirc-server/master/art/commands.png)

The only non-directional command implemented so far is the command "`X`" which
shuts down the Raspberry Pi server.


Known issues
------------

The video stream has about 1 second delay due to the transport through the
network and the encoding/decoding of the video.

The control can be delayed due to the network communication.

Only the control channel 1 works.

Only the "Combo PWM mode" is implemented.

There might be other issues. Please report them on the [project
site](https://github.com/jtyr/legoirc-server).


License
-------

This software is licensed by the MIT License which can be found in the file
[LICENSE](http://github.com/jtyr/legoirc-server/blob/master/LICENSE).
