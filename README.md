# NEUC - New Encrypted UDP Chat

A simple program for text chat utilizing UDP hole punching and encryption.

#### Getting Started

To install you need libtomcrypt and tomsfastmath. These can be found on github at https://github.com/libtom/libtomcrypt and https://github.com/libtom/tomsfastmath. You have to install tomsfastmath first, and then install libtomcrypt to use TFM as its math provider. Details in its documentation. Then run `make` and `sudo make install`. To run the client, execute `neuc_client` and enter the IP of the target hole-punching server. Then, enter your "connection key" or a string that you have shared with someone else for the server to match clients. Then the cursor should move back to the smaller bottom text box and you can start chatting.

#### Using the Server

The server runs on port 5544. Run using `neuc_server`.

#### Development Timeline

When time allows, I will be adding to and improving NEUC. The table below shows some features that I hope to be adding soon.

| Priority | Networking                                                   | Encryption                   | Client                                                  |
| -------- | ------------------------------------------------------------ | ---------------------------- | ------------------------------------------------------- |
| Higher   | - Create a queue<br />system for requests.                   | - Hash connection<br />keys. | - Add status bar.<br />- Disconnect and <br />reconnect |
| Lower    | - Ping the server<br />multiple times <br />instead of once. | - RSA-4098?                  | - Add color.<br />- Chat logging <br />option.          |
