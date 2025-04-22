# Multithreaded Web Server

## Overview
This project is a multithreaded web server written in C. It is designed to handle multiple client requests simultaneously using a thread pool and a work-stealing approach for efficient thread scheduling. The server supports basic HTTP functionality and can serve files from a specified directory. It also includes SSL support for secure communication.

## Features
- **Multithreading**: Uses `pthreads` to handle multiple client requests concurrently.
- **Work-Stealing Thread Pool**: Implements a custom thread pool with a work-stealing approach to balance the workload among threads.
- **Custom Thread IDs**: Assigns custom thread IDs specific to the server, ignoring internal thread ID numbers.
- **Configuration File**: Reads server settings from a `server.conf` file.
- **SSL Support**: Provides secure communication using SSL/TLS.
- **Graceful Shutdown**: Handles termination signals (`SIGINT`, `SIGTERM`) to shut down the server cleanly.

## Project Structure
- `src/main.c`: Entry point of the server. Initializes and starts the server.
- `src/server.c`: Core server logic, including request handling and response generation.
- `src/http.c`: HTTP protocol handling.
- `src/thread_pool.c`: Implementation of the thread pool with work-stealing.
- `src/ssl_utils.c`: SSL/TLS utilities for secure communication.
- `src/config.c`: Configuration file parsing.
- `src/logger.c`: Logging utilities for debugging and monitoring.
- `Makefile`: Build instructions for the project.
- `server.conf`: Configuration file for server settings.


## Setting Up SSL

To enable SSL for secure communication in this project, follow these steps:

1. **Generate SSL Certificate and Key**:
   Use the `openssl` command to generate a self-signed certificate and private key:
   ```bash
   openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout server.key -out server.crt

## Configuration
The server reads its configuration from the `server.conf` file.
   
```plaintext
port = 8080
max_clients = 45
num_workers = 4
buffer_size = 1024
base_dir = ./html
ssl_cert = server.crt
ssl_key = server.key
```