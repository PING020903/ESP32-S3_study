# Embed the server root certificate into the final binary
idf_component_register(SRCS "My_https_request.c" "My_scheme.c" "My_WIFI_init.c" "My_timer_init.c" "My_GPIO_init.c" "ESP_S3_project.c"
                    INCLUDE_DIRS "." "../include"
                    EMBED_TXTFILES server_root_cert.pem local_server_cert.pem)
