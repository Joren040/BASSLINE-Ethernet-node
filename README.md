<div align="center">
<h1>BASSLINE-Ethernet-node</h1>
<p><i>DIY ethernet-to-DMX interface that uses a raspberry pi Pico (RP2040) microcontroller</i></p>
<hr>
</div>

<h2><p align="center">📌 Project Philosophy</p></h2>
<div>The <b>BASSLINE-Ethernet-node</b> was created when the lighting software in my local bar pulled support for it's DMX USB-Dongle.</div>
<div>So i decided i would design them a new one, they wanted something simple and affordable, but would have liked 2 output universe.
It is then i stumbeld upon <a href="https://github.com/tmingos/Pico-ArtNet-DMX-Node">Pico-ArtNet-DMX-Node by tmingos</a> and decide to move from USB to ArtNet.</div>
<div>This project was a great inspiration i liked the onboard OLED and Dual DMX output,and use of a cheap readily available components like the Raspberry Pi pico microntroller.</div>
<div>I did not like that it was depended on a wireless connection, they needed a physical connection so i moved to Ethernet.</div>
<div>The onboard OLED would be very under utilized in a wired scenario so i added 4 buttons an build an entire menu.</div> 
<div>Here the user can set IP, universe and protocol, because i was also able to add s/ACN.</div>

<h2><p align="center">⚡ Key Features</p></h2>

<ul>
<li><b>Pi Pico:</b> Chosen for its affordable price and the dual-core RP2040’s incredible PIO features for DMX timing.</li>
<li><b>Wired Stability:</b> Chose the WIZnet W5500 to add Ethernet connectivity to the Pico.</li>
<li><b>Custom PCBs:</b> Designed 2 Custom PCBs, Main board and menu board.</li>
<li><b>Onboard menu:</b> Set the IP address, universes and protocol on the device directly.</li>
<li><b>Isolated:</b> Used optocouplers and power isolator to separate DMX signal from microcontroller logic.</li>
<li><b>Eeprom:</b> Added an memory chip to store data between power cycles.</li>
<li><b>Compatibillty</b> Tested with MagicQ (in ArtNet & s/ACN modes), OLA & my <a href=https://github.com/Joren040/BASSLINE-Recorder>BASSLINE Recorder</a>.</li>
</ul>

<h2><p align="center">🛠 Dependencies</p></h2>
<p><i>Special thanks to the creators of these libraries</i></p>
<ul>
<li>Pico-DMX by Jostein (Tested with 3.0.1)</li>
<li>ArtNet Hideaki Tai (Tested with 0.8.0)</li>
<li>sACN by Stefan Staub (Tested with 1.1.0)</li>
<li>arduino-libraries Ethernet (Tested with 2.0.2)</li>
<li>Adafruit SSD1306 (Tested with 2.5.10)</li>
<li>Adafruit GFX Library (Tested with 1.11.9)</li>
</ul>

<h2><p align="center">🚀 Installation & Usage</p></h2>

<p>This project has been written in PlatfromIO</p>

<p><b>1. create a new platformIO project</b></p>
<p>select the raspberry Pi Pico as device and Arduino as framework.</p>

<p><b>2. copy the contents to the correct files</b></p>
<p>
<div>Start with the contents of the <code>platform.ini</code> file.Then add the 2 files to the inlcude folder <code>config.h</code> & <code>pgmspace.h</code></div>
<div>At last locate the src folder and copy over the code of the <code>main.cpp</code> file, and build the project. It might take some for the nessecary libraries to install.</div>
</p>

<p><b>3. flash the Pi pico</b></p>
<p>
<div>After the build, connect the Raspberry Pi Pico to pc and find it's COM port, select and flash  with the code</div>
</p>


