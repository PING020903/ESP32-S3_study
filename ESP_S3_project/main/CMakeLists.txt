# Embed the server root certificate into the final binary
idf_component_register(SRCS "My_LED_init.c" "My_https_request.c" "My_timer_init.c" "My_GPIO_init.c" "My_usb_device.c" "app_main.c"
                       INCLUDE_DIRS "./include"
                       EMBED_TXTFILES server_root_cert.pem local_server_cert.pem)
