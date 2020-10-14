#include "esp_sleep.h"
#include "common.h"

// sniffer code taken from - https://github.com/ETS-PoliTO/esp32-sniffer/blob/master/main/main.c

typedef struct {
	int16_t fctl; //frame control
	int16_t duration; //duration id
	uint8_t da[6]; //receiver address
	uint8_t sa[6]; //sender address
	uint8_t bssid[6]; //filtering address
	int16_t seqctl; //sequence control
	unsigned char payload[]; //network data
} __attribute__((packed)) wifi_mgmt_hdr;

cListStruct cList[10];
int cListIndex = 0;

static const char *TAG = "Sniffer ";

void snifferTask(void *pvParameter)
{
	int sleep_time = 2*1000;  // 10 seconds
	//char macStr[18];

	ESP_LOGI(TAG, "[SNIFFER] Sniffer Task created");
	// Save 1 entry in the Whielist - aseem
	/*
	cList[0].cl[0]=0x8c; cList[0].cl[1]=0xb8; cList[0].cl[2]=0x4a; 
	cList[0].cl[3]=0x3a; cList[0].cl[4]=0x9d; cList[0].cl[5]=0x66; //aseem
	strcpy(cList[0].name, "aseem"); cListIndex = 1;
		snprintf(cList[0].wl, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
         	cList[0].cl[0], cList[0].cl[1], cList[0].cl[2], 
         	cList[0].cl[3], cList[0].cl[4], cList[0].cl[5]);
	ESP_LOGI(TAG, "Saved MAC String Aseem: %s", cList[0].wl);
	*/

	wifi_sniffer_init();

	while(true){
		vTaskDelay(sleep_time / portTICK_PERIOD_MS);
	}
}

void wifi_sniffer_init()
{
	tcpip_adapter_init();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg)); //allocate resource for WiFi driver

	const wifi_country_t wifi_country = {
			.cc = "CN",
			.schan = 1,
			.nchan = 13,
			.policy = WIFI_COUNTRY_POLICY_AUTO
	};
	ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country)); //set country for channel range [1, 13]
	//ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	//ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
	//ESP_ERROR_CHECK(esp_wifi_start());

	// Only Probe pkts working for now.
	const wifi_promiscuous_filter_t filt = {
			.filter_mask = WIFI_EVENT_MASK_AP_PROBEREQRECVED 
	};
	// | WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_EVENT_MASK_ALL
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filt)); //set filter mask
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler)); //callback function
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true)); //set 'true' the promiscuous mode

	esp_wifi_set_channel(CONFIG_CHANNEL, WIFI_SECOND_CHAN_NONE); //only set the primary channel
}

void checkMacs(wifi_mgmt_hdr *mgmt) {
    
    char temp[200];
    char macStr[18];
    /* for (int i=0; i<3;i++) {
		if (memcmp(&mgmt->sa, &cList[i].cl, sizeof(mgmt->sa)) == 0) {
		}
	}
	*/
	// Convert sniffed MAC into string
	snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
         	mgmt->sa[0], mgmt->sa[1], mgmt->sa[2], 
         	mgmt->sa[3], mgmt->sa[4], mgmt->sa[5]);
	//ESP_LOGI(TAG, "Sniffed MAC String: %s", macStr);
	for (int i=0; i<=cListIndex;i++) {
		if (strcmp(macStr, cList[i].wl) == 0) {
			ESP_LOGI(TAG, "Found: %s", cList[i].name);
			strcpy(temp, "Found: ");
			strcat(temp, cList[i].name);
			if (cList[i].sentNote == false) {
				ESP_LOGI(TAG, "Sending 1st Note");
				cList[i].sentNote = true;
				wifi_send_mqtt(temp);
			} else {
				ESP_LOGI(TAG, "Not Sending Note");
			}
		}
	}
}


IRAM_ATTR void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
	int pkt_len, fc; //sn=0;
	char ssid[SSID_MAX_LEN] = "\0", hash[MD5_LEN] = "\0", htci[5] = "\0";
	uint8_t ssid_len;
	time_t ts;

	wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
	wifi_mgmt_hdr *mgmt = (wifi_mgmt_hdr *)pkt->payload;

	fc = ntohs(mgmt->fctl);

	if((fc & 0xFF00) == 0x4000){ //only look for probe request packets
		time(&ts);

		ssid_len = pkt->payload[25];
		if(ssid_len > 0)
			get_ssid(pkt->payload, ssid, ssid_len);

		pkt_len = pkt->rx_ctrl.sig_len;
		get_hash(pkt->payload, pkt_len-4, hash);

		/* if(CONFIG_VERBOSE){
			ESP_LOGI(TAG, "Dump");
			dumb(pkt->payload, pkt_len);
		}*/

		//sn = get_sn(pkt->payload);

		get_ht_capabilites_info(pkt->payload, htci, pkt_len, ssid_len);

		/* ESP_LOGI(TAG, "ADDR=%02x:%02x:%02x:%02x:%02x:%02x, "
				"SSID=%s, "
				"TIMESTAMP=%d, "
				"HASH=%s, "
				"RSSI=%02d, ",
				mgmt->sa[0], mgmt->sa[1], mgmt->sa[2], mgmt->sa[3], mgmt->sa[4], mgmt->sa[5],
				ssid,
				(int)ts,
				hash,
				pkt->rx_ctrl.rssi);
		*/
		checkMacs(mgmt);
		//save_pkt_info(mgmt->sa, ssid, ts, hash, pkt->rx_ctrl.rssi, sn, htci);
	}
}

void get_hash(unsigned char *data, int len_res, char hash[MD5_LEN])
{
	/*
	uint8_t pkt_hash[16];

	md5((uint8_t *)data, len_res, pkt_hash);

	sprintf(hash, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			pkt_hash[0], pkt_hash[1], pkt_hash[2], pkt_hash[3], pkt_hash[4], pkt_hash[5],
			pkt_hash[6], pkt_hash[7], pkt_hash[8], pkt_hash[9], pkt_hash[10], pkt_hash[11],
			pkt_hash[12], pkt_hash[13], pkt_hash[14], pkt_hash[15]);
	*/
}


void get_ssid(unsigned char *data, char ssid[SSID_MAX_LEN], uint8_t ssid_len)
{
	int i, j;

	for(i=26, j=0; i<26+ssid_len; i++, j++){
		ssid[j] = data[i];
	}

	ssid[j] = '\0';
}

int get_sn(unsigned char *data)
{
	int sn;
    char num[5] = "\0";

	sprintf(num, "%02x%02x", data[22], data[23]);
    sscanf(num, "%x", &sn);

    return sn;
}

void get_ht_capabilites_info(unsigned char *data, char htci[5], int pkt_len, int ssid_len)
{
	int ht_start = 25+ssid_len+19;

	/* 1) data[ht_start-1] is the byte that says if HT Capabilities is present or not (tag length).
	 * 2) I need to check also that i'm not outside the payload: if HT Capabilities is not present in the packet,
	 * for this reason i'm considering the ht_start must be lower than the total length of the packet less the last 4 bytes of FCS */

	if(data[ht_start-1]>0 && ht_start<pkt_len-4){ //HT capabilities is present
		if(data[ht_start-4] == 1) //DSSS parameter is set -> need to shift of three bytes
			sprintf(htci, "%02x%02x", data[ht_start+3], data[ht_start+1+3]);
		else
			sprintf(htci, "%02x%02x", data[ht_start], data[ht_start+1]);
	}
}

void dumb(unsigned char *data, int len)
{
	unsigned char i, j, byte;

	for(i=0; i<len; i++){
		byte = data[i];
		printf("%02x ", data[i]);

		if(((i%16)==15) || (i==len-1)){
			for(j=0; j<15-(i%16); j++)
				printf(" ");
			printf("| ");
			for(j=(i-(i%16)); j<=i; j++){
				byte = data[j];
				if((byte>31) && (byte<127))
					printf("%c", byte);
				else
					printf(".");
			}
			printf("\n");
		}
	}
}

void save_pkt_info(uint8_t address[6], char *ssid, time_t timestamp, char *hash, int8_t rssi, int sn, char htci[5])
{
	//int stime;

	//stime = get_start_timestamp();
	//printf("%d\n", stime);

	printf("%02x:%02x:%02x:%02x:%02x:%02x %s %d %s %02d %d %s\n",
			address[0], address[1], address[2], address[3], address[4], address[5],
			ssid,
			(int)timestamp,
			hash,
			rssi,
			sn,
			htci);

}

int get_start_timestamp()
{
	int stime;
	time_t clk;

	time(&clk);
	stime = (int)clk - (int)clk % 60;

	return stime;
}