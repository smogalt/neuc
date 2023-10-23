# NEUC - New Encrypted UDP Chat

A simple program for text chat utilizing UDP hole punching and encryption.

#### Getting Started

To install you need libtommath and tomsfastmath. Use ```apt install libtommath-dev``` and ```apt install libtomcrypt-dev``` respective. Then run `make` and `sudo make install`. To run the client, run `neuc_client` and enter the IP of the target server. Then, enter your "connection key" or a string that you have shared with someone else for the server to match clients. When the circle in the bottom right turns into a check you are connected.

#### Using the Server

The server runs on port 5544. Run using `neuc_server`.
