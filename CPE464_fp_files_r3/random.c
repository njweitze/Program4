if (argc > 1) {
        if (inet_pton(AF_INET, argv[1], &server_addr.sin6_addr) <= 0) {
            if (inet_pton(AF_INET, argv[1], &IP_v4) <= 0) {
                perror("Invalid IP address");
                exit(EXIT_FAILURE);
            } else {
                char *msk = "::ffff:";
                char *converted_IP_v6 = malloc((sizeof(argv[1]) + sizeof(msk) + 1) * sizeof(char));
                strcpy(converted_IP_v6, msk);
                strcpy(converted_IP_v6, argv[1]);
                inet_pton(AF_INET6, converted_IP_v6, &server_addr.sin6_addr);
                free(converted_IP_v6);
            }
        }
    } else {
        server_addr.sin6_addr = in6addr_any; // bind to all interfaces
    }