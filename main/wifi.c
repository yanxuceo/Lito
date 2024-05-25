
#include "wifi.h"


static esp_err_t list_files_handler(httpd_req_t *req);
static esp_err_t download_file_handler(httpd_req_t *req);
static esp_err_t sync_time_handler(httpd_req_t *req);


static const char *TAG = "wifi";

// WiFi credentials
const char* WIFI_SSID = "XIAO_AP";
const char* WIFI_PASS = "12345678"; //password must be at least 8 digits


static esp_err_t list_files_handler(httpd_req_t *req) {
    char dir_path[MAX_PATH_LENGTH];
    snprintf(dir_path, sizeof(dir_path), "%s%s", MOUNT_POINT, AUDIO_DIR);

    DIR *dir = opendir(dir_path);
    if (!dir) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    struct dirent *entry;
    char file_list[1024] = "";
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            strcat(file_list, entry->d_name);
            strcat(file_list, "\n");
        }
    }
    closedir(dir);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, file_list, strlen(file_list));
    return ESP_OK;
}


static esp_err_t download_file_handler(httpd_req_t *req) {
    char file_path[MAX_PATH_LENGTH];
    snprintf(file_path, sizeof(file_path), "%s%s", MOUNT_POINT, AUDIO_DIR);

    char filepath[64];
    strlcpy(filepath, file_path, sizeof(filepath));
    strlcat(filepath, "/", sizeof(filepath));
    strlcat(filepath, req->uri + 10, sizeof(filepath));

    FILE *file = fopen(filepath, "r");
    if (!file) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    return ESP_OK;
}


static esp_err_t sync_time_handler(httpd_req_t *req) {
    char content[100];
    int ret;

    // Read the request content
    if ((ret = httpd_req_recv(req, content, sizeof(content))) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    // Parse the received time (expected in ISO 8601 format)
    struct tm tm;
    if (strptime(content, "%Y-%m-%dT%H:%M:%S", &tm) == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid time format");
        return ESP_FAIL;
    }

    // Convert to time_t and set the system time
    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    httpd_resp_sendstr(req, "Time synchronized");
    return ESP_OK;
}


httpd_uri_t list_files = {
    .uri       = "/list",
    .method    = HTTP_GET,
    .handler   = list_files_handler,
    .user_ctx  = NULL
};


httpd_uri_t download_file = {
    .uri       = "/download/*",
    .method    = HTTP_GET,
    .handler   = download_file_handler,
    .user_ctx  = NULL
};


// URI handler structure
httpd_uri_t sync_time = {
    .uri       = "/sync_time",
    .method    = HTTP_POST,
    .handler   = sync_time_handler,
    .user_ctx  = NULL
};


void start_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    // Copy the SSID and password to the wifi_config structure
    strncpy((char *)wifi_config.ap.ssid, WIFI_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    strncpy((char *)wifi_config.ap.password, WIFI_PASS, sizeof(wifi_config.ap.password));

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}


void start_http_server(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &list_files);
        httpd_register_uri_handler(server, &download_file);
        httpd_register_uri_handler(server, &sync_time); 
    }

    ESP_LOGI(TAG, "HTTP server started");
}
