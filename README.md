# Multi-Client-Chat-server

A multi-client server is an application in which one client can send messages to other
clients connected to the server. It supports broadcasting the message to all the clients
connected to the server. Clients can create groups with other clients that are connected to the
server. This application is made by using C language and by using socket programming.

## Used TF-IDF vectorizer and KNN to detect hate speech
When one client sends a message to other clients, the message will be first checked to detect whether it is hate speech or not; if the message is hate speech, then the message will not be sent
and after getting a certain number of hate speech messages from a client, it will be removed from the server.
