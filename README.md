# netproghw2

The purpose of this homework is to write a (fake) Huffman-coded FTP server & client. There's only three commands required in the client:

```
login <IP> <port>
send <filename>
logout
```

For server, it does not interact with user. Server should save file and corresponding Huffman Code table sent from client.

The requirement of the homework:

* Server
  - Accept connection
  - Print client address and port when connection established
  - Print message on termination
  - Uncompress and save file sent from client
  - Save Huffman Code table to file

* Client
  - Three commands: login, send, logout must be implemented
  - File sent should be compressed using Huffman Coding
  - Huffman Code should be transferred to server, too

* Common
  - Both binary and ASCII text must be able to transfer correctly
  - Fixed-length Huffman Coding is required.
  - Variable-length Huffman Coding is a bonus.
  - Program must be implemented using Unix socket programming
  - A Makefile must be included.
