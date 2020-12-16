# Millionaire

## Table of contents
* [General Info](#general-info)
* [Members/Contributors](#member-(3))
* [Technology](#technology)
* [Setup](#setup)

=======================================================

## General Info
Vietnamese version of gameshow "Who wants to be a millionaire" written in C and run directly on terminal.

## Member (3)
<a href="https://github.com/minhld99/Millionaire/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=minhld99/Millionaire" />
</a>

## Technology
Socket Programming in C
* TCP Protocol
* Multithreading
* Select
* SQLite3 (Database)

## Setup
<b>For running on local:</b>
- Uncomment this line `servaddr.sin_addr.s_addr = htonl(INADDR_ANY);`
- Open the same port on server and client side:  
```$ ./server 5000```

<b>If server & client on different network</b>: 
- Port forwarding and clone needed...
- Minh's Device External IPv4 Address: `14.248.75.42` (For testing purposes only)

1. <b> Open server (on Minh's Device) </b>
```
$ cd server
$ make
$ ./server 8000
```

2. <b> Run client app (On Linh/Ha's Device) </b>
```
$ cd ../client
$ make
$ ./client
$ (using server IP 14.248.75.42 & port 5000)
```
