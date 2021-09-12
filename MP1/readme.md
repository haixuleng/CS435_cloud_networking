## Task 1: HTTP Client

### Brief description: [Coursera](https://www.coursera.org/learn/cs-435-cn/programming/)
In this task, you will implement a simple HTTP client. The client should be able to GET files correctly from standard web servers. 

HTTP uses TCP, so you can use Beej's client.c and server.c as a base.

HTTP Communication Basics
The client sends an HTTP GET request to the server.

The server replies with a response based on the request of the client (e.g. acknowledge the validity of the request with an OK response OR reply with a file not found error response etc.).

Based on the response from the server, the client takes further action (e.g. store the message content in a file OR redirect to a different address with a new GET request, etc.)

### How to test:
You can test the correctness of your client by running a local http server using the following python command line script on a separate terminal window in the relevant directory:

python3 -m http.server 8000  

### testing cases:

Client Requirements
You should write your code in C or C++. Note that our autograder supports up to C++14. This program should not print anything to stdout. Your code should compile to generate an executable named http_client, with the usage specified as follows:

`./http_client http://hostname[:port]/path_to_file`

For example:

`./http_client http://illinois.edu/index.html`

`./http_client http://localhost:5678/somedir/somefile.html`

Your client should send an HTTP GET request based on the first argument it receives. Your client should then write the message body of the response received from the server to a file named output. 

The file can be any type of content that you could get via HTTP, including text, HTML, images, and more. Note that you don't need to specify the content-type in your GET request, but you need to be able to receive the file in different types. The file size may vary from a few bytes up to hundreds of MBs.

Optional: your client should also be able to handle HTTP 301 redirections (not graded).

Nonexistent Files
If the server returns a 404 error for a non-existent file, your client should only write “FILENOTFOUND” to the output file.

Invalid Arguments
The client should be able to handle any invalid arguments as follows.  First, if the protocol specified is not HTTP, the program should write “INVALIDPROTOCOL” to the output file. For example, the following command should be handled as an invalid protocol argument:

`./http_client htpt://localhost/file`

Second, if the client is unable to connect to the specified host, the program should write “NOCONNECTION” to the output file.
