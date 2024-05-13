#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "stm32l475e_iot01.h"
// Embox SPI driver
//#include <drivers/spi.h>
//#include "/home/andrew/embox/src/drivers/spi/stm32/stm32_spi.h"
//#define WIFI_SPI_BUS	3

#define WIFI_RESET_MODULE()	do {	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);\
					HAL_Delay(100);\
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);\
					HAL_Delay(50);\
					} while(0)

#define WIFI_CHIP_SELECT()	do {	HAL_GPIO_WritePin( GPIOE, GPIO_PIN_0, GPIO_PIN_RESET );\
					} while(0)

#define WIFI_CHIP_DESELECT()	do {	HAL_GPIO_WritePin( GPIOE, GPIO_PIN_0, GPIO_PIN_SET );\
					} while(0)
					
#define WIFI_IS_CMDDATA_READY()	(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1) == GPIO_PIN_SET)

#define BUFFLEN 2048

// usage i+=foofunc(b,i);
static int foofunc(char *b, int i) {
	if (0x00 <= b[i] && b[i] <= 0x7f) return 1;
	return sprintf(b+i, "\\x%02X", (int)b[i]);
}

int main() {
	printf("B-L475E-IOT01A WiFi testing\n");

	// WiFi module init (WakeUp, DRdy, Rst)
	GPIO_InitTypeDef GPIO_Init;
	memset(&GPIO_Init, 0, sizeof(GPIO_Init));

	__HAL_RCC_SPI3_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

 	/* configure Wake up pin */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	GPIO_Init.Pin       = GPIO_PIN_13;
	GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_Init );

	/* configure Data ready pin */
	GPIO_Init.Pin       = GPIO_PIN_1;
	GPIO_Init.Mode      = GPIO_MODE_INPUT;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOE, &GPIO_Init );

	/* configure Reset pin */
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	GPIO_Init.Pin       = GPIO_PIN_8;
	GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_Init.Alternate = 0;
	HAL_GPIO_Init(GPIOE, &GPIO_Init);

	/* configure SPI soft NSS pin for WiFi module */
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);
	GPIO_Init.Pin       =  GPIO_PIN_0;
	GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOE, &GPIO_Init);

	/* configure SPI CLK pin */
	GPIO_Init.Pin       =  GPIO_PIN_10;
	GPIO_Init.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_Init.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC, &GPIO_Init);

	/* configure SPI MISO pin */
	GPIO_Init.Pin       = GPIO_PIN_11;
	GPIO_Init.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init.Pull      = GPIO_PULLUP;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_Init.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC,&GPIO_Init);

	/* configure SPI MOSI pin */
	GPIO_Init.Pin       = GPIO_PIN_12;
	GPIO_Init.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_Init.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC, &GPIO_Init);

	/* SPI hardware init */
	SPI_HandleTypeDef hspi;
	memset(&hspi, 0, sizeof(hspi));
	hspi.Instance = SPI3;
	hspi.Init.Mode              = SPI_MODE_MASTER;
	hspi.Init.Direction         = SPI_DIRECTION_2LINES;
	hspi.Init.DataSize          = SPI_DATASIZE_16BIT;
	hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;
	hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
	hspi.Init.NSS               = SPI_NSS_SOFT;
	hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 80/8= 10MHz (Inventek WIFI module supports up to 20MHz)*/
	hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
	hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
	hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
	hspi.Init.CRCPolynomial     = 0;

	if(HAL_SPI_Init(&hspi) != HAL_OK) {
		printf("HAL_SPI_Init failure\n");
		return -1;
	}

	// Reset WiFi module
	clock_t t0;
	char b[BUFFLEN]; b[BUFFLEN-1]=0;
	int len;
	char tail[2]={'\r','\n'};
	char in[2];
	int c;
	HAL_StatusTypeDef  Status;

//	t0 = clock(); HAL_Delay(10); printf("HAL_Delay(10) = %d ms\n", clock() - t0);

	WIFI_RESET_MODULE();

	t0 = clock(); while (!WIFI_IS_CMDDATA_READY()) if ((clock() - t0) > 10000) { // TODO define!
		printf("Error waiting initial CMD/DATA READY sign\n");
		return -1;
	}    
	printf("CMD/DATA READY sign apears in %d ms after reset\n", clock() - t0);

	WIFI_CHIP_SELECT();
	usleep(15);	// ??? datasheet requires 15 us

	c = 0;
	for (int i=0; (i < 1000) && WIFI_IS_CMDDATA_READY(); i++) {
		t0 = clock();
		in[0]='\n'; in[1]='\n';
		Status = HAL_SPI_Receive(&hspi, (uint8_t*)in, 1, HAL_MAX_DELAY);
		if((clock() - t0) > 100) {WIFI_CHIP_DESELECT();printf("Error reading initial prompt for >100 ms\n");return -1;}
		else if (Status != HAL_OK) {WIFI_CHIP_DESELECT();printf("Error reading initial prompt HAL_SPI_Receive returns %d\n", (int)Status);return -1;}
		else if (c >= BUFFLEN-1) {WIFI_CHIP_DESELECT();printf("Error reading initial prompt, c=%d\n", c);return -1;}    
		if (in[0] != 0x15 && in[0] != '\r' && in[0] != '\n') b[c++]=in[0];
		if (in[1] != 0x15 && in[0] != '\r' && in[0] != '\n') b[c++]=in[1];
printf("i=%3d, c=%03d, in[0]=%02x, in[1]=%02x\n", i, c, (int)in[0], (int)in[1]);
	}
	b[c]=0;
	printf("Initial prompt from WiFi module: \"%s\"\n", b);

	WIFI_CHIP_DESELECT();

	if((b[0] != '>') || (b[1] != ' ')) {
		printf("Initial prompt not valid\n");
		return -1;
	}
		
	t0 = clock(); while (!WIFI_IS_CMDDATA_READY()) if ((clock() - t0) > 1000) { // TODO define!
		printf("Error waiting CMD/DATA READY sign\n");
		return -1;
	}    
	printf("CMD/DATA READY sign apears in %d ms\n", clock() - t0);

	printf("WiFi ready!\n");

	// AT command loop
	while (1) {
		printf("WiFi module>");
		fflush(stdout);

		//scanf("%s", b), gets(b) - doesn't work
		for (int i=0; i<BUFFLEN; i++) {
			b[i]=getchar();
			if(b[i] == '\r' || i == BUFFLEN-1) {
				b[i]=0;
				putchar('\r');
				if (b[0] == '\t') {
					sprintf(b, "S3=%04d", strlen(b+8));
					printf("WiFi module>%s`%s\n", b, b+8);
					b[7]='\r';
					break;
				}
				putchar('\n');
				break;
			}
			if (i == 0 && b[i] == '\t') {
				i+=7;
				printf("\rWiFi module>S3=0000`");
				fflush(stdout);
				continue;
			}
			putchar(b[i]);
			if (b[i] == '`') b[i]='\r';
			if (b[i] == '~') b[i]='\n';
			fflush(stdout);
		}

		if (b[0] == 'x') break;
		len = strlen(b);
		if (!len) continue;
//		if (!len) goto read_mess;
		b[len]='\r';
		len++;

		WIFI_CHIP_SELECT();
		t0=clock();
		Status = HAL_SPI_Transmit(&hspi, (uint8_t*)b, len/2, 0xFF);
		if(((clock() - t0) > 100) || (Status != HAL_OK)) { // TODO define max for clock() - t0
			WIFI_CHIP_DESELECT();
			printf("Error sending command\n");
			return -1;
		}    
		if (len & 1) {
			HAL_SPI_Transmit(&hspi, (uint8_t*)tail, 1, 0xFF);
			if(((clock() - t0) > 100) || (Status != HAL_OK)) { // TODO define max for clock() - t0
				WIFI_CHIP_DESELECT();
				printf("Error sending command\n");
				return -1;
			}
		}
		
		WIFI_CHIP_DESELECT();
		//usleep(1000);
		t0 = clock(); while (WIFI_IS_CMDDATA_READY()) if ((clock() - t0) > 10000) { // TODO define!
			printf("Error waiting CMD/DATA READY sign disappears\n");
			return -1;
		}    
		if (clock()>t0) printf("CMD/DATA READY sign disapears in %d ms\n", clock() - t0);
//read_mess:
		t0 = clock(); while (!WIFI_IS_CMDDATA_READY()) if ((clock() - t0) > 10000) { // TODO define!
			printf("Error waiting answer CMD/DATA READY sign\n");
			return -1;
		}    
		if (clock()>t0) printf("CMD/DATA READY sign apears in %d ms\n", clock() - t0);

		WIFI_CHIP_SELECT();

		c=0;
		for (int i=0; (i < 1000) && WIFI_IS_CMDDATA_READY(); i++) {
			t0=clock();
			in[0]='\n'; in[1]='\n';
			Status = HAL_SPI_Receive(&hspi, (uint8_t*)in, 1, 0xFF);
			if((clock() - t0) > 100) {WIFI_CHIP_DESELECT();printf("Error reading answer for >100 ms\n");return -1;}
			else if (Status != HAL_OK) {WIFI_CHIP_DESELECT();printf("Error reading answer HAL_SPI_Receive returns %d\n", (int)Status);return -1;}
			if (in[0] != 0x15) {b[c]=in[0]; c+=foofunc(b, c);} //b[c++]=in[0];
			if (in[1] != 0x15) {b[c]=in[1]; c+=foofunc(b, c);} //b[c++]=in[1];
			if (c >= BUFFLEN-9) break; //-3
		}
		WIFI_CHIP_DESELECT();
		if (c >= 4 && b[c-4] == '\r' && b[c-3] == '\n' && b[c-2] == '>' && b[c-1] == ' ') c-=4;
		if (c >= 4 && b[c-4] == '\r' && b[c-3] == '\n' && b[c-2] == 'O' && b[c-1] == 'K') c-=4;
		if (c >= 2 && b[c-2] == '\r' && b[c-1] == '\n') c-=2;
		b[c]=0;
		c=0;
		if (b[c] == '\r' && b[c+1] == '\n') c+=2;
		if (b[c] == '\r' && b[c+1] == '\n') c+=2;
		puts(b+c);
		fflush(stdout);

		t0 = clock(); while (!WIFI_IS_CMDDATA_READY()) if ((clock() - t0) > 2000) { // TODO define!
			printf("Error waiting CMD/DATA READY sign\n");
			return -1;
		}    
		if (clock()>t0) printf("CMD/DATA READY sign apears in %d ms\n", clock() - t0);
	}
	
	return 0;
}
